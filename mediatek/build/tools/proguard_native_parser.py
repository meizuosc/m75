#!/usr/bin/python
import sys, os
import re
from optparse import OptionParser

JNI_GET_METHOD = 'GetMethodID'

def getJavaMethods(filePath):
    inputFile = file(filePath)
    lineNo = 1
    javaMethods = list()
    
    # fine JNI_GET_METHOD line by line
    for line in inputFile:
        funcStart = line.find(JNI_GET_METHOD)
        funcCall = ""
        if funcStart != -1:
            #print 'find %s at %d' % (JNI_GET_METHOD, lineNo)
            funcEnd = line.rfind(';')
            funcCall = line[funcStart:funcEnd]
        
        if funcCall != "":
            pattern = r'(\w[\w\d_]*)\((.*)\)$'
            match = re.match(pattern, funcCall)
            if match:
                grps = list(match.groups())
                if len(grps) == 2:
                    args = grps[1].split(',')
                    if len(args) == 3:
                        javaFuncName = args[1].strip(' ')
                        if javaFuncName[0] == '\"':
                            javaFuncName = javaFuncName.strip('\"')
                            location = '%s:%d' % (filePath, lineNo)
                            javaMethods.append((location, javaFuncName))

        lineNo += 1

    return javaMethods

def writeProGuard(javaMethods, filePath):
    try:
        file = open(filePath+'/proguard_native', 'w')
        for location, method in javaMethods:
            file.write('# view ' + location + '\n')
            file.write('-keepclassmembers class * {\n')
            file.write('  *** ' + method + '(...);\n')
            file.write('}\n\n')
    except:
        print 'open error' + filePath

def main():
    parser = OptionParser(usage="usage: %prog input_directory",version="%prog 1.0")
    (options, args) = parser.parse_args()
    if len(args) != 2:
        parser.print_help()
        sys.exit(1)

    curDir = os.getcwd()
    inDir = curDir + '/' + args[0]
    outDir = curDir + '/' + args[1]
    print 'input dir: ' + inDir
    print 'output dir: ' + outDir
    
    javaMethods = list()
    for root, dirs, files in os.walk(inDir):
        for file in files:
            if file.endswith('.cpp'):
                filePath = root + '/' + file
                methods = getJavaMethods(filePath)
                if methods:
                    #print filePath
                    #print methods
                    for location, method in methods:
                        javaMethods.append((location, method))

    writeProGuard(javaMethods, outDir)

if __name__ == "__main__":
    main()
