 #!/bin/bash

G_NUM=
G_TYPE="_type"

#+++++++++++++++++++++++++++++++++++++++++++++++++++++++
function gen_language_segment()
{
    awk -F"\"" '{print $3}' $1>$1"_temp"
    sort -k2n $1"_temp"|uniq > $1$G_TYPE
    awk -F" " '{$1="";$2="";$3="";$4="";$NF="";print $0}' $1$G_TYPE>$1"_temp"
    sed 's/\,//g' $1"_temp">$1$G_TYPE
    $(pwd)"/mediatek/build/tools/single.awk" $1$G_TYPE>$1"_temp"
    awk '{print "FileName LineNum StringID "$0}' $1"_temp">$1$G_TYPE
    rm $1"_temp"
    G_NUM=$(awk '{print NF}' $1$G_TYPE)
}

function gen_interest_info()
{
    awk -F'[:\"]' '{$3="";print $0}' $1>$1"_temp"
    sed 's/\,//g' $1"_temp">$1"_space"
    awk -F' ' '{$4="";$5="";$6="";$7="";$NF="";print $0}' $1"_space">$1"_temp"
    cat $1"_temp"|tr -s " ">$1
    rm $1"_space"
    rm $1"_temp"
}

function gen_interest_line()
{
    TEMP_FILE_RESULT=$3"/$(echo $4|awk -F'_' '{print $1}')"
    if [ -f $TEMP_FILE_RESULT ]; then
        rm $TEMP_FILE_RESULT
    fi
    if [ ]; then
        awk '$0 ~ /\'$1'/ {print}' $2"/$4">${TEMP_FILE_RESULT}
    else
       awk '{if($0 ~ /\'$1'/){a=$0;b=NR}else{if(NR>1&&NR==(b+1)&&(!($0 ~ /translate=\"false\"/))){print a}}}' $2"/$4">${TEMP_FILE_RESULT}
    fi
    gen_language_segment $TEMP_FILE_RESULT
    gen_interest_info $TEMP_FILE_RESULT
}
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++
function gen_fin_result()
{
    MID_FILE_NAME=$1"/$(echo $3|awk -F'_' '{print $1}')"
    MID_FILE_TYPE=$MID_FILE_NAME$G_TYPE
    FIN_FILE_NAME=$2"/$(echo $3|awk -F'_' '{print $1}').csv"
    if [ -f $FIN_FILE_NAME ]; then
        rm $FIN_FILE_NAME
    fi

    printf "\xEF\xBB\xBF" >$FIN_FILE_NAME
    if [ ]; then
        awk -F' ' '{print $0}' $MID_FILE_TYPE > $FIN_FILE_NAME
        awk -f $(pwd)"/mediatek/build/tools/compare.awk" -v cl=$G_NUM -v tf=$MID_FILE_TYPE $MID_FILE_NAME >> $FIN_FILE_NAME
    else
        echo "FileName,LineNum,StringID,en-rUS,zh-rCN,zh-rTW,es,pt,ru,fr,de,tr,vi,ms,in,th,it,ar,hi,bn,ur,fa,pt-rPT,nl,el,hu,tl,ro,cs,ko,km-rKH,iw,my-rMM">$MID_FILE_TYPE
        awk -F' ' '{print $0}' $MID_FILE_TYPE > $FIN_FILE_NAME
        G_NUM=`awk -F',' '{print NF}' $MID_FILE_TYPE`
        awk -f $(pwd)"/mediatek/build/tools/comparedel.awk" -v cl=$G_NUM -v tf=$MID_FILE_TYPE $MID_FILE_NAME >> $FIN_FILE_NAME
    fi

    echo "">> $FIN_FILE_NAME
    echo "Unused-resource">> $FIN_FILE_NAME
    awk -F'[: ]' '$0 ~ /\[UnusedResources]/ {OFS = ",";if ($2!="") {print $1,$2,$8,$13} else {print $1,"(File)",$7,$12}}' $4"/$3" >> $FIN_FILE_NAME
}

#+++++++++++++++++++++++++++++++++++++++++++++++++++++++
RESULT_FILES_LIST_FILE=$(pwd)"/path.txt"
if [ -z "$1" -o ! -d "$1" ]; then
    RESULT_FILE_PATH=$(pwd)"/path"
else
    RESULT_FILE_PATH=$1
fi
if [ -f $RESULT_FILES_LIST_FILE ]; then
    rm $RESULT_FILES_LIST_FILE
fi
if [ ! -d "$RESULT_FILE_PATH" ]; then
    echo -e "\033[31mFile path error, please check it first\033[0m"
    exit 127
fi
ls $RESULT_FILE_PATH > $RESULT_FILES_LIST_FILE

RESULT_MID_PATH=$(pwd)"/mid"
if [ -d $RESULT_MID_PATH ];then
   rm -r $RESULT_MID_PATH
fi
mkdir $RESULT_MID_PATH

RESULT_FIN_PATH=$(pwd)"/ResultCsv"
if [ -d $RESULT_FIN_PATH ];then
   rm -r $RESULT_FIN_PATH
fi
mkdir $RESULT_FIN_PATH

CSV_FILES_LIST_FILE=$(pwd)"/csv.txt"
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++
declare -a RESULT_FILES_NAME_ARRAY=($(cat $RESULT_FILES_LIST_FILE|tr -d " "))
for i in ${RESULT_FILES_NAME_ARRAY[@]};do
    gen_interest_line "[MissingTranslation]" $RESULT_FILE_PATH $RESULT_MID_PATH $i
    gen_fin_result $RESULT_MID_PATH $RESULT_FIN_PATH $i $RESULT_FILE_PATH
done

ls $RESULT_FIN_PATH | tr '\n' ' ' | sed 's/ $//' > $CSV_FILES_LIST_FILE
#"/usr/bin/python" $(pwd)"/csv2xls.py" $(pwd)"/csv.txt" $RESULT_FIN_PATH $RESULT_FIN_PATH"/result_all"
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++
if [ -d $RESULT_MID_PATH ];then
   rm -r $RESULT_MID_PATH
fi
if [ -f $RESULT_FILES_LIST_FILE ]; then
    rm $RESULT_FILES_LIST_FILE
fi
if [ -f $CSV_FILES_LIST_FILE ]; then
    rm $CSV_FILES_LIST_FILE
fi
#echo $LINENO
#read FLAG
