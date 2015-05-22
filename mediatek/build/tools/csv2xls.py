#!/user/bin/python

import datetime
import time
import os
import sys
import xlwt

def csv2xls(namefile,resfpath,xlsfname):
    print 'converting xls ... '
    ff = open(namefile)
    fn = ff.readline()
    xls=xlwt.Workbook()
    for j in fn.split(' '):
        f = open(resfpath+os.sep+j)
        x = 0
        y = 0
        sheet = xls.add_sheet(j,cell_overwrite_ok=True)
        while True:
            line = f.readline()
            if not line:
                break
            for i in line.split(','):
                item=i.strip().decode('ascii')
                sheet.write(x,y,item)
                y += 1
            x += 1
            y = 0
        f.close()
    ff.close()
    xls.save(xlsname+'.xls')

if __name__ == "__main__":
    namefile = sys.argv[1]
    resfpath = sys.argv[2]
    xlsfname  = sys.argv[3]
    csv2xls(namefile,resfpath,xlsfname)


