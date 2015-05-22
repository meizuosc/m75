/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_pcm_io2_dai.c
 *
 * Project:
 * --------
 *   Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio DAI data1 uplink IO2 to O11
 *
 * Author:
 * -------
 * Tina Tsai
 *
 *------------------------------------------------------------------------------
 * $Revision: #1 $
 * $Modtime:$
 * $Log:$
 *
 *
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_pcm_common.h"

//information about
static AFE_MEM_CONTROL_T  *DAI_Control_context;
static struct snd_dma_buffer *io2_dai_dma_buf  = NULL;
static AudioDigtalI2S *mAudioDigitalI2S = NULL;
static DEFINE_SPINLOCK(auddrv_ULInCtl_lock);

/*
 *    function implementation
 */
static void StartAudioio2_daiHardware(struct snd_pcm_substream *substream);
static void StopAudioio2_daiHardware(struct snd_pcm_substream *substream);
void StartAudioio2_daiAnalogHardware(void);
void StopAudioio2_daiAnalogHardware(void);
static int mtk_io2_dai_probe(struct platform_device *pdev);
static int mtk_io2_dai_pcm_close(struct snd_pcm_substream *substream);
static int mtk_asoc_io2_dai_pcm_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_io2_dai_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_io2_dai_hardware =
{
    .info = (SNDRV_PCM_INFO_MMAP |
    SNDRV_PCM_INFO_INTERLEAVED |
    SNDRV_PCM_INFO_RESUME |
    SNDRV_PCM_INFO_MMAP_VALID),
    .formats =      SND_SOC_STD_MT_FMTS,
    .rates =        SOC_HIGH_USE_RATE,
    .rate_min =     SOC_HIGH_USE_RATE_MIN,
    .rate_max =     SOC_HIGH_USE_RATE_MAX,
    .channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
    .channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
    .buffer_bytes_max = UL1_MAX_BUFFER_SIZE,
    .period_bytes_max = UL1_MAX_BUFFER_SIZE,
    .periods_min =      UL1_MIN_PERIOD_SIZE,
    .periods_max =      UL1_MAX_PERIOD_SIZE,
    .fifo_size =        0,
};

static void StopAudioio2_daiHardware(struct snd_pcm_substream *substream)
{
    pr_debug("StopAudioio2_daiHardware \n");

    // here to set interrupt
    SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, false);
    SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DAI, false);
    EnableAfe(false);
}


static void StartAudioio2_daiHardware(struct snd_pcm_substream *substream)
{
    pr_debug("StartAudioio2_daiHardware \n");

    SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DAI, AFE_WLEN_16_BIT);
    SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DAI, AFE_WLEN_16_BIT);
    SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O09);
    SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O10);

    // here to set interrupt
    SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, substream->runtime->period_size);
    SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, substream->runtime->rate);
    SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, true);

    SetSampleRate(Soc_Aud_Digital_Block_MEM_DAI, substream->runtime->rate);
    SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DAI, true);

    EnableAfe(true);

}

static int mtk_io2_dai_pcm_prepare(struct snd_pcm_substream *substream)
{
    //printk("mtk_io2_dai_pcm_prepare substream->rate = %d  substream->channels = %d \n", substream->runtime->rate, substream->runtime->channels);
    return 0;
}

static int mtk_io2_dai_alsa_stop(struct snd_pcm_substream *substream)
{
    AFE_BLOCK_T *Dai_Block = &(DAI_Control_context->rBlock);
    pr_debug("mtk_io2_dai_alsa_stop \n");
    StopAudioio2_daiHardware(substream);
    RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DAI,substream);
    return 0;
}

static kal_int32 Previous_Hw_cur = 0;
static snd_pcm_uframes_t mtk_io2_dai_pcm_pointer(struct snd_pcm_substream *substream)
{
    kal_int32 HW_memory_index = 0;
    kal_int32 HW_Cur_ReadIdx = 0;
    kal_uint32 Frameidx = 0;
    AFE_BLOCK_T *Awb_Block = &(DAI_Control_context->rBlock);
    PRINTK_AUD_DAI("mtk_io2_dai_pcm_pointer Awb_Block->u4WriteIdx;= 0x%x \n", Awb_Block->u4WriteIdx);
    if (DAI_Control_context->interruptTrigger == 1)
    {
        // get total bytes to copysinewavetohdmi
        Frameidx = audio_bytes_to_frame(substream , Awb_Block->u4WriteIdx);
        return Frameidx;

        HW_Cur_ReadIdx = Afe_Get_Reg(AFE_AWB_CUR);
        if (HW_Cur_ReadIdx == 0)
        {
            PRINTK_AUD_DAI("[Auddrv] mtk_awb_pcm_pointer  HW_Cur_ReadIdx ==0 \n");
            HW_Cur_ReadIdx = Awb_Block->pucPhysBufAddr;
        }
        HW_memory_index = (HW_Cur_ReadIdx - Awb_Block->pucPhysBufAddr);
        Previous_Hw_cur = HW_memory_index;
        PRINTK_AUD_DAI("[Auddrv] mtk_awb_pcm_pointer =0x%x HW_memory_index = 0x%x\n", HW_Cur_ReadIdx, HW_memory_index);
        DAI_Control_context->interruptTrigger = 0;
        return (HW_memory_index >> 2);
    }
    return (Previous_Hw_cur >> 2);

}

static void SetDAIBuffer(struct snd_pcm_substream *substream,
                         struct snd_pcm_hw_params *hw_params)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    AFE_BLOCK_T *pblock = &DAI_Control_context->rBlock;
    pr_debug("SetDAIBuffer\n");
    pblock->pucPhysBufAddr =  runtime->dma_addr;
    pblock->pucVirtBufAddr =  runtime->dma_area;
    pblock->u4BufferSize = runtime->dma_bytes;
    pblock->u4SampleNumMask = 0x001f;  // 32 byte align
    pblock->u4WriteIdx     = 0;
    pblock->u4DMAReadIdx    = 0;
    pblock->u4DataRemained  = 0;
    pblock->u4fsyncflag     = false;
    pblock->uResetFlag      = true;
    pr_debug("u4BufferSize = %d pucVirtBufAddr = %p pucPhysBufAddr = 0x%x\n",
           pblock->u4BufferSize, pblock->pucVirtBufAddr, pblock->pucPhysBufAddr);
    // set dram address top hardware
    Afe_Set_Reg(AFE_DAI_BASE , pblock->pucPhysBufAddr , 0xffffffff);
    Afe_Set_Reg(AFE_DAI_END  , pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1), 0xffffffff);

}

static int mtk_io2_dai_pcm_hw_params(struct snd_pcm_substream *substream,
                                     struct snd_pcm_hw_params *hw_params)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
    int ret = 0;
    pr_debug("mtk_io2_dai_pcm_hw_params \n");

    dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
    dma_buf->dev.dev = substream->pcm->card->dev;
    dma_buf->private_data = NULL;
    pr_debug("io2_dai_dma_buf = %p io2_dai_dma_buf->area = %p\n", io2_dai_dma_buf, io2_dai_dma_buf->area);

    if (io2_dai_dma_buf->area)
    {
        pr_debug("mtk_io2_dai_pcm_hw_params io2_dai_dma_buf->area\n");
        runtime->dma_bytes = io2_dai_dma_buf->bytes;
        runtime->dma_area = io2_dai_dma_buf->area;
        runtime->dma_addr = io2_dai_dma_buf->addr;
        runtime->buffer_size = io2_dai_dma_buf->bytes;
    }
    else
    {
        pr_debug("mtk_io2_dai_pcm_hw_params snd_pcm_lib_malloc_pages\n");
        ret =  snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
    }
    pr_debug("mtk_io2_dai_pcm_hw_params dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
           runtime->dma_bytes, runtime->dma_area, runtime->dma_addr);

    pr_debug("runtime->hw.buffer_bytes_max = 0x%x \n", runtime->hw.buffer_bytes_max);
    SetDAIBuffer(substream, hw_params);

    pr_debug("dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
           substream->runtime->dma_bytes, substream->runtime->dma_area, substream->runtime->dma_addr);
    return ret;
}

static int mtk_io2_dai_pcm_hw_free(struct snd_pcm_substream *substream)
{
    pr_debug("mtk_io2_dai_pcm_hw_free \n");
    if (io2_dai_dma_buf->area)
    {
        return 0;
    }
    else
    {
        return snd_pcm_lib_free_pages(substream);
    }
}

static struct snd_pcm_hw_constraint_list constraints_sample_rates =
{
    .count = ARRAY_SIZE(soc_normal_supported_sample_rates),
    .list = soc_normal_supported_sample_rates,
};

static int mtk_io2_dai_pcm_open(struct snd_pcm_substream *substream)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    int ret = 0;
    AudDrv_Clk_On();

    pr_debug("%s \n", __func__);
    DAI_Control_context = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DAI);

    runtime->hw = mtk_io2_dai_hardware;
    memcpy((void *)(&(runtime->hw)), (void *)&mtk_io2_dai_hardware , sizeof(struct snd_pcm_hardware));
    pr_debug("runtime->hw->rates = 0x%x \n ", runtime->hw.rates);

    ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
                                     &constraints_sample_rates);
    ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
    if (ret < 0)
    {
        printk("snd_pcm_hw_constraint_integer failed\n");
    }

    pr_debug("mtk_io2_dai_pcm_open runtime rate = %d channels = %d \n", runtime->rate, runtime->channels);
    if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
    {
        pr_debug("SNDRV_PCM_STREAM_CAPTURE mtkalsa_io2_dai_constraints\n");
    }
    else
    {

    }

    if (ret < 0)
    {
        pr_debug("mtk_io2_dai_pcm_close\n");
        mtk_io2_dai_pcm_close(substream);
        return ret;
    }
    pr_debug("mtk_io2_dai_pcm_open return\n");
    return 0;
}

static int mtk_io2_dai_pcm_close(struct snd_pcm_substream *substream)
{
    AudDrv_Clk_Off();
    return 0;
}

static int mtk_io2_dai_alsa_start(struct snd_pcm_substream *substream)
{
    pr_debug("mtk_io2_dai_alsa_start \n");
    SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DAI, substream);
    StartAudioio2_daiHardware(substream);
    return 0;
}

static int mtk_io2_dai_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
    pr_debug("mtk_io2_dai_pcm_trigger cmd = %d\n", cmd);

    switch (cmd)
    {
        case SNDRV_PCM_TRIGGER_START:
        case SNDRV_PCM_TRIGGER_RESUME:
            return mtk_io2_dai_alsa_start(substream);
        case SNDRV_PCM_TRIGGER_STOP:
        case SNDRV_PCM_TRIGGER_SUSPEND:
            return mtk_io2_dai_alsa_stop(substream);
    }
    return -EINVAL;
}

static bool CheckNullPointer(void *pointer)
{
    if (pointer == NULL)
    {
        pr_debug("CheckNullPointer pointer = NULL");
        return true;
    }
    return false;
}

static int mtk_io2_dai_pcm_copy(struct snd_pcm_substream *substream,
                                int channel, snd_pcm_uframes_t pos,
                                void __user *dst, snd_pcm_uframes_t count)
{

    AFE_MEM_CONTROL_T *pDAI_MEM_ConTrol = NULL;
    AFE_BLOCK_T  *Dai_Block = NULL;
    char *Read_Data_Ptr = (char *)dst;
    ssize_t DMA_Read_Ptr = 0 , read_size = 0, read_count = 0;
    struct snd_pcm_runtime *runtime = substream->runtime;
    unsigned long flags;

    PRINTK_AUD_DAI("mtk_io2_dai_pcm_copy pos = %lucount = %lu \n ", pos, count);
    // get total bytes to copy
    count = audio_frame_to_bytes(substream , count);

    // check which memif nned to be write
    pDAI_MEM_ConTrol = DAI_Control_context;
    Dai_Block = &(pDAI_MEM_ConTrol->rBlock);

    if (pDAI_MEM_ConTrol == NULL)
    {
        printk("cannot find MEM control !!!!!!!\n");
        msleep(50);
        return 0;
    }

    if (Dai_Block->u4BufferSize <= 0)
    {
        msleep(50);
        pr_debug("Dai_Block->u4BufferSize <= 0  =%d\n", Dai_Block->u4BufferSize);
        return 0;
    }

    if (CheckNullPointer((void *)Dai_Block->pucVirtBufAddr))
    {
        pr_debug("CheckNullPointer  pucVirtBufAddr = %p\n", Dai_Block->pucVirtBufAddr);
        return 0;
    }

    spin_lock_irqsave(&auddrv_ULInCtl_lock, flags);
    if (Dai_Block->u4DataRemained >  Dai_Block->u4BufferSize)
    {
        PRINTK_AUD_DAI("AudDrv_MEMIF_Read u4DataRemained=%x > u4BufferSize=%x" , Dai_Block->u4DataRemained, Dai_Block->u4BufferSize);
        Dai_Block->u4DataRemained = 0;
        Dai_Block->u4DMAReadIdx   = Dai_Block->u4WriteIdx;
    }
    if (count >  Dai_Block->u4DataRemained)
    {
        read_size = Dai_Block->u4DataRemained;
    }
    else
    {
        read_size = count;
    }

    DMA_Read_Ptr = Dai_Block->u4DMAReadIdx;
    spin_unlock_irqrestore(&auddrv_ULInCtl_lock, flags);

    PRINTK_AUD_DAI("AudDrv_MEMIF_Read finish0, read_count:%x, read_size:%x, u4DataRemained:%x, u4DMAReadIdx:0x%x, u4WriteIdx:%x \r\n",
                   read_count, read_size, Dai_Block->u4DataRemained, Dai_Block->u4DMAReadIdx, Dai_Block->u4WriteIdx);

    if (DMA_Read_Ptr + read_size < Dai_Block->u4BufferSize)
    {
        if (DMA_Read_Ptr != Dai_Block->u4DMAReadIdx)
        {
            pr_debug("AudDrv_MEMIF_Read 1, read_size:%x, DataRemained:%x, DMA_Read_Ptr:0x%x, DMAReadIdx:%x \r\n",
                   read_size, Dai_Block->u4DataRemained, DMA_Read_Ptr, Dai_Block->u4DMAReadIdx);
        }

        if (copy_to_user((void __user *)Read_Data_Ptr, (Dai_Block->pucVirtBufAddr + DMA_Read_Ptr), read_size))
        {

            printk("AudDrv_MEMIF_Read Fail 1 copy to user Read_Data_Ptr:%p, pucVirtBufAddr:%p, u4DMAReadIdx:0x%x, DMA_Read_Ptr:0x%x,read_size:%x", Read_Data_Ptr, Dai_Block->pucVirtBufAddr, Dai_Block->u4DMAReadIdx, DMA_Read_Ptr, read_size);
            return 0;
        }

        read_count += read_size;
        spin_lock(&auddrv_ULInCtl_lock);
        Dai_Block->u4DataRemained -= read_size;
        Dai_Block->u4DMAReadIdx += read_size;
        Dai_Block->u4DMAReadIdx %= Dai_Block->u4BufferSize;
        DMA_Read_Ptr = Dai_Block->u4DMAReadIdx;
        spin_unlock(&auddrv_ULInCtl_lock);

        Read_Data_Ptr += read_size;
        count -= read_size;

        PRINTK_AUD_DAI("AudDrv_MEMIF_Read finish1, copy size:%x, u4DMAReadIdx:0x%x, u4WriteIdx:%x, u4DataRemained:%x \r\n",
                       read_size, Dai_Block->u4DMAReadIdx, Dai_Block->u4WriteIdx, Dai_Block->u4DataRemained);
    }

    else
    {
        uint32 size_1 = Dai_Block->u4BufferSize - DMA_Read_Ptr;
        uint32 size_2 = read_size - size_1;

        if (DMA_Read_Ptr != Dai_Block->u4DMAReadIdx)
        {

            pr_debug("AudDrv_MEMIF_Read 2, read_size1:%x, DataRemained:%x, DMA_Read_Ptr:0x%x, DMAReadIdx:%x \r\n",
                   size_1, Dai_Block->u4DataRemained, DMA_Read_Ptr, Dai_Block->u4DMAReadIdx);
        }
        if (copy_to_user((void __user *)Read_Data_Ptr, (Dai_Block->pucVirtBufAddr + DMA_Read_Ptr), size_1))
        {

            printk("AudDrv_MEMIF_Read Fail 2 copy to user Read_Data_Ptr:%p, pucVirtBufAddr:%p, u4DMAReadIdx:0x%x, DMA_Read_Ptr:0x%x,read_size:%x",
                   Read_Data_Ptr, Dai_Block->pucVirtBufAddr, Dai_Block->u4DMAReadIdx, DMA_Read_Ptr, read_size);
            return 0;
        }

        read_count += size_1;
        spin_lock(&auddrv_ULInCtl_lock);
        Dai_Block->u4DataRemained -= size_1;
        Dai_Block->u4DMAReadIdx += size_1;
        Dai_Block->u4DMAReadIdx %= Dai_Block->u4BufferSize;
        DMA_Read_Ptr = Dai_Block->u4DMAReadIdx;
        spin_unlock(&auddrv_ULInCtl_lock);


        PRINTK_AUD_DAI("AudDrv_MEMIF_Read finish2, copy size_1:%x, u4DMAReadIdx:0x%x, u4WriteIdx:0x%x, u4DataRemained:%x \r\n",
                       size_1, Dai_Block->u4DMAReadIdx, Dai_Block->u4WriteIdx, Dai_Block->u4DataRemained);

        if (DMA_Read_Ptr != Dai_Block->u4DMAReadIdx)
        {

            pr_debug("AudDrv_AWB_Read 3, read_size2:%x, DataRemained:%x, DMA_Read_Ptr:0x%x, DMAReadIdx:%x \r\n",
                   size_2, Dai_Block->u4DataRemained, DMA_Read_Ptr, Dai_Block->u4DMAReadIdx);
        }
        if (copy_to_user((void __user *)(Read_Data_Ptr + size_1), (Dai_Block->pucVirtBufAddr + DMA_Read_Ptr), size_2))
        {

            printk("AudDrv_MEMIF_Read Fail 3 copy to user Read_Data_Ptr:%p, pucVirtBufAddr:%p, u4DMAReadIdx:0x%x , DMA_Read_Ptr:0x%x, read_size:%x", Read_Data_Ptr, Dai_Block->pucVirtBufAddr, Dai_Block->u4DMAReadIdx, DMA_Read_Ptr, read_size);
            return read_count << 2;
        }

        read_count += size_2;
        spin_lock(&auddrv_ULInCtl_lock);
        Dai_Block->u4DataRemained -= size_2;
        Dai_Block->u4DMAReadIdx += size_2;
        DMA_Read_Ptr = Dai_Block->u4DMAReadIdx;
        spin_unlock(&auddrv_ULInCtl_lock);

        count -= read_size;
        Read_Data_Ptr += read_size;

        PRINTK_AUD_DAI("AudDrv_MEMIF_Read finish3, copy size_2:%x, u4DMAReadIdx:0x%x, u4WriteIdx:0x%x u4DataRemained:%x \r\n",
                       size_2, Dai_Block->u4DMAReadIdx, Dai_Block->u4WriteIdx, Dai_Block->u4DataRemained);
    }

    return read_count >> 2;
}

static int mtk_io2_dai_pcm_silence(struct snd_pcm_substream *substream,
                                   int channel, snd_pcm_uframes_t pos,
                                   snd_pcm_uframes_t count)
{
    pr_debug("dummy_pcm_silence \n");
    return 0; /* do nothing */
}


static void *dummy_page[2];

static struct page *mtk_io2_dai_pcm_page(struct snd_pcm_substream *substream,
                                         unsigned long offset)
{
    pr_debug("%s \n", __func__);
    return virt_to_page(dummy_page[substream->stream]); /* the same page */
}


static struct snd_pcm_ops mtk_afe_io2_dai_ops =
{
    .open =     mtk_io2_dai_pcm_open,
    .close =    mtk_io2_dai_pcm_close,
    .ioctl =    snd_pcm_lib_ioctl,
    .hw_params =    mtk_io2_dai_pcm_hw_params,
    .hw_free =  mtk_io2_dai_pcm_hw_free,
    .prepare =  mtk_io2_dai_pcm_prepare,
    .trigger =  mtk_io2_dai_pcm_trigger,
    .pointer =  mtk_io2_dai_pcm_pointer,
    .copy =     mtk_io2_dai_pcm_copy,
    .silence =  mtk_io2_dai_pcm_silence,
    .page =     mtk_io2_dai_pcm_page,
};

static struct snd_soc_platform_driver mtk_soc_platform =
{
    .ops        = &mtk_afe_io2_dai_ops,
    .pcm_new    = mtk_asoc_io2_dai_pcm_new,
    .probe      = mtk_afe_io2_dai_probe,
};

static int mtk_io2_dai_probe(struct platform_device *pdev)
{
    pr_debug("mtk_io2_dai_probe\n");
    if (pdev->dev.of_node)
    {
        dev_set_name(&pdev->dev, "%s", MT_SOC_IO2_DAI_PCM);
    }

    pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
    return snd_soc_register_platform(&pdev->dev,
                                     &mtk_soc_platform);
}
static u64 io2_dai_pcm_dmamask = DMA_BIT_MASK(32);

static int mtk_asoc_io2_dai_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
    struct snd_card *card = rtd->card->snd_card;
    int ret = 0;
    if (!card->dev->dma_mask)
    {
        card->dev->dma_mask = &io2_dai_pcm_dmamask;
    }
    if (!card->dev->coherent_dma_mask)
    {
        card->dev->coherent_dma_mask = DMA_BIT_MASK(32);
    }
    pr_debug("mtk_asoc_io2_dai_pcm_new \n");
    return ret;
}


static int mtk_afe_io2_dai_probe(struct snd_soc_platform *platform)
{
    pr_debug("mtk_afe_io2_dai_probe\n");
    AudDrv_Allocate_mem_Buffer(Soc_Aud_Digital_Block_MEM_DAI, UL1_MAX_BUFFER_SIZE);
    io2_dai_dma_buf =  Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DAI);
    mAudioDigitalI2S =  kzalloc(sizeof(AudioDigtalI2S), GFP_KERNEL);
    return 0;
}


static int mtk_io2_dai_remove(struct platform_device *pdev)
{
    pr_debug("%s\n", __func__);
    snd_soc_unregister_platform(&pdev->dev);
    return 0;
}

static struct platform_driver mtk_afe_io2_dai_driver =
{
    .driver = {
        .name = MT_SOC_IO2_DAI_PCM,
        .owner = THIS_MODULE,
    },
    .probe = mtk_io2_dai_probe,
    .remove = mtk_io2_dai_remove,
};

static struct platform_device *soc_mtkafe_io2_dai_dev;

static int __init mtk_soc_io2_dai_platform_init(void)
{
    int ret = 0;
    pr_debug("%s\n", __func__);
    soc_mtkafe_io2_dai_dev = platform_device_alloc(MT_SOC_IO2_DAI_PCM, -1);
    if (!soc_mtkafe_io2_dai_dev)
    {
        return -ENOMEM;
    }

    ret = platform_device_add(soc_mtkafe_io2_dai_dev);
    if (ret != 0)
    {
        platform_device_put(soc_mtkafe_io2_dai_dev);
        return ret;
    }

    ret = platform_driver_register(&mtk_afe_io2_dai_driver);
    return ret;
}
module_init(mtk_soc_io2_dai_platform_init);

static void __exit mtk_soc_platform_exit(void)
{

    pr_debug("%s\n", __func__);
    platform_driver_unregister(&mtk_afe_io2_dai_driver);
}

module_exit(mtk_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");


