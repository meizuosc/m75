#! /usr/bin/env python
__author__ = 'mtk81316'
import os, sys, re, getopt, commands, string
import xml.dom.minidom as xdom


def main(argv):
    try:
        opts, args = getopt.getopt(argv, "hs:d:p:x:", ["help", "sourcedir=", "destdir=", "project=", "xmlpath="])
    except getopt.GetoptError:
        Usage()
        sys.exit(2)

    sourceDir, destDir, project, rel_list = parse_opt(opts)
    if not sourceDir or not destDir or not project or not rel_list:
        Usage()
    platformlist = search_platform(sourceDir)
    src_list = calculate_split_list(sourceDir, platformlist, rel_list)
    if os.path.exists(destDir):
        (outStatus, val) = commands.getstatusoutput("rm -rf %s" % destDir)
        if outStatus:
            print >>sys.stderr, "error for clean destination!"
            sys.exit(3)
    for item in src_list:
	print >> sys.stdout,"%s" % item
        check_exist(os.path.join(sourceDir, item))
        split_gpl(os.path.join(sourceDir, item), os.path.join(destDir, item))
        check_exist(os.path.join(destDir, item))
    print >> sys.stdout, "buildable packages split Successfully!"
# for remove copyright
    (outStatus, val) = commands.getstatusoutput("find %s -type f |xargs perl %s/mediatek/build/tools/extraction/rm_legal.pl" %(destDir, destDir))
    if outStatus:
        print >> sys.stderr,"error when first round remove copyright!"
    (outStatus, val) = commands.getstatusoutput("find %s -type f |xargs perl %s/mediatek/build/tools/extraction/rm_legal2.pl" % (destDir, destDir))
    if outStatus:
        print >> sys.stderr,"error when second round remove copyright!"
    print >> sys.stdout, "Remove copyright Successfully!"
def check_exist(path):
    """check the path exists"""
    if not os.path.exists(path):
	if not os.path.isfile(path):
	    if not os.path.islink(path):
                print >>sys.stderr, "%s does not exits" % path
                sys.exit(3)

def split_gpl(src, dest):
    """get the gpl packages from source to target"""
    path_status = check_exist(src)
    if not path_status:
        if not os.path.exists(dest):
            if os.path.isfile(src) or os.path.islink(src):
                if not os.path.exists(os.path.dirname(dest)):
		    os.makedirs(os.path.dirname(dest))
                print >>sys.stdout, "sync %s %s ... " % (src, dest)
                (outStatus, val) = commands.getstatusoutput("rsync -a %s %s " % (src, dest))
            else:
                os.makedirs(dest)
                print >> sys.stdout, "sync %s %s ... " % (src, dest)
                (outStatus, val) = commands.getstatusoutput("rsync -a --delete --force %s/ %s " % (src, dest))

            if outStatus:
                print >> sys.stderr, "rsync %s %s error, please make sure your environment is ok!" % (src, dest)
                sys.exit(13)
            (outStatus, val) = commands.getstatusoutput("rm -rf %s" % src)
            if outStatus:
                print >> sys.stderr, "split error, can not removed %s" % src
                sys.exit(13)


def calculate_split_list(src, platform_list, xmlpath):
    """calculate the gpl list"""
    dom = XmlDom(xmlpath)
    src_list = []
    for item in dom.getDirList():
        if os.path.exists(os.path.join(src, item)):
            src_list.append(item)
    for item in dom.getFileList():
        if os.path.exists(os.path.join(src, item)):
            src_list.append(item)
    for platform in platform_list:
        if os.path.exists(os.path.join(src, "mediatek/platform/%s/kernel" % platform)):
            src_list.append("mediatek/platform/%s/kernel" % platform)
        else:
            print >> sys.stderr, "[Error] no platform kernel source ?"
            sys.exit(3)
    return src_list


class XmlDom(object):
    def __init__(self, xml):
        self.xmlDom = xdom.parse(xml)

    def getRoot(self):
        return self.xmlDom.documentElement

    def getDirList(self):
        root = self.getRoot()
        dirElement = root.getElementsByTagName("Dirlist")[0].getElementsByTagName("Dir")
        dirList = map(str, [item.firstChild.nodeValue for item in dirElement if item.firstChild is not None])
        return dirList

    def getFileList(self):
        root = self.getRoot()
        dirElement = root.getElementsByTagName("Filelist")[0].getElementsByTagName("File")
        dirList = map(str, [item.firstChild.nodeValue for item in dirElement if item.firstChild is not None])
        return dirList


def parse_opt(opts):
    srcDir = ""
    desDir = ""
    project = ""
    rel_list = ""
    for opt, arg in opts:
        if opt in ("-s", "--sourcedir"):
            srcDir = arg
        if opt in ("-d", "--destdir"):
            desDir = arg
        if opt in ("-p", "--project"):
            project = arg
        if opt in ("-h", "--help"):
            Usage()
        if opt in ("-x", "-xmlpath"):
            rel_list = arg
    return srcDir, desDir, project, rel_list


def get_platform(src):
    """query the current platform"""
    pattern = [re.compile("^([^=\s]+)\s*=\s*(.+)$"),
               re.compile("^([^=\s]+)\s*=$"),
               re.compile("\s*#")]
    config = {}
    config["MTK_PLATFORM"]=""
    ff = open(src, "r")
    for line in ff.readlines():
        result = (filter(lambda x: x, [x.search(line) for x in pattern]) or [None])[0]
        if not result: continue
        name, value = None, None
        if len(result.groups()) == 0: continue
        name = result.group(1)
        try:
            value = result.group(2)
        except IndexError:
            value = ""
        config[name] = value.strip()

#    print >> sys.stdout, "[Platform]: %s" % (config["MTK_PLATFORM"]).lower()
    return (config["MTK_PLATFORM"]).lower()

def search_platform(src):
    """ query all the ProjectConfig.mk for readind platformlist"""
    platform_list=[]
    PrjCfgMkList = map(lambda x:x.rstrip(),list(os.popen("find %s/mediatek/config -follow -name ProjectConfig.mk" % src)))
    for prjcfg in PrjCfgMkList:
        platform = get_platform(prjcfg)
	if platform:
	    if platform not in platform_list:
 	       platform_list.append(platform)
	       print >> sys.stdout, "[PLATFORM]: %s" % platform
    return platform_list

def Usage():
    print """Usage:
                   -h, --help=:          show the Usage of commandline argments;
		   -s, --sourcedir=:     source path
                   -d, --destdir=:       destination path;
                   -x, --xmlpath=:       gpl release file/folder list
example:
                   python gplpuller.py -s $inhousepath -d $gplpath -p $project -x $xmlpath
                   python gplpuller.py --sourcedir=$inhousepath --destdir=$gplpath --project=$project --xmplpath=$xmlpath
          """
    sys.exit(0)

if __name__ == "__main__":
    main(sys.argv[1:])
