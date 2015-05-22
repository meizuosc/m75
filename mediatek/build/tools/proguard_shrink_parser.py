#!/usr/bin/python

import sys
from xml.dom import minidom
from optparse import OptionParser

def writeFile(outputFile, className, classMembers, newClass):
    if className != None:
        if newClass:
            #print 'remove class %s' % (className)
            outputFile.write('remove class %s\n' % (className))
        elif len(classMembers) > 0:
            #print 'remove members of class %s' % (className)
            outputFile.write('remove members of class %s\n' % (className))
            for member in classMembers:
                #print member
                outputFile.write('%s\n' % (member))

def main():
    parser = OptionParser(usage="usage: %prog [AndroidManifest.xml] [usage.txt] [important_shrink.txt]",version="%prog 1.0")
    (options, args) = parser.parse_args()
    if len(args) != 3:
        parser.print_help()
        return

    xmldoc = minidom.parse(args[0])
    items = xmldoc.getElementsByTagName('manifest')
    packageName = items[0].attributes['package'].value
    
    try:
        shrinkFile = open(args[1], "r")
    except:
        print 'open shrink: %s error' % (args[1])
        return
    try:
        outputFile = open(args[2], "w")
    except:
        print 'open output: %s error' % (args[2])
        return
    
    lines = shrinkFile.readlines()
    findPackage = False
    newClass = False
    className = None
    classMembers = []
    for line in lines:
        line = line.rstrip('\n')
        if findPackage and line[0] == ' ':
            if newClass:
                classMembers = []
                newClass = False
            if "static final" not in line:
                classMembers.append(line)
        elif line.startswith(packageName) or line.startswith("com.mediatek"):
            writeFile(outputFile, className, classMembers, newClass)
            findPackage = True
            newClass = True
            className = line
        else:
            findPackage = False

    writeFile(outputFile, className, classMembers, newClass)

    shrinkFile.close()
    outputFile.close()

if __name__ == "__main__":
    main()
