#!/usr/bin/python
################################################################
# SEMTK_policy_check.py
#
# This file is parsing *init.rc and /system/* executable file
#
# Input:
# Assume /system/* is generated first
#
# Output:
# print warning log into  ./semtk_policy_check/semtk_policy_check.log
#
################################################################

import sys
import os 
import re
import commands
import shutil

debug=1
print_tag="[SEMTK_Check_Policy] : "

#output policy and log dir
semtk_policy_check_dir="SEAndroid"
semtk_policy_check_obj_dir=""

#out_dir
out_dir=""

#output log file
semtk_policy_log_filename="semtk.log"

#output policy
semtk_file_contexts_filename="file_contexts"
semtk_dev_type_filename="mtk_device.te"
semtk_file_type_filename="mtk_file.te"

#type enforcement file directory
seandroid_root_policy_dir="external/sepolicy"
semtk_root_policy_dir=""

product_name=""
alps_path=""
load_path=""

#scan executable file path 
exec_type_postfix = "exec"
out_sbin_path = "root/sbin"  #from out/target/product/XXX/ 
phone_sbin_dir = "/sbin"

out_system_bin_path = "system/bin"
phone_system_bin_dir = "/system/bin"

out_system_xbin_path = "system/xbin"
phone_system_xbin_dir = "/system/xbin" 

out_vendor_bin_path  = "system/vendor/bin"
phone_vendor_bin_dir  = "/system/vendor/bin" 

out_vendor_xbin_path = "system/vendor/xbin"
phone_vendor_xbin_dir = "/system/vendor/xbin"

#scan init file path
out_init_dir="root"

#policy setting 
phone_data_dir = "/data"
data_type_postfix="data_file"
data_file_type_set="file_type, data_file_type"

phone_data_misc_dir = "/data/misc"
data_misc_type_postfix="data_file"

phone_dev_dir = "/dev"
dev_type_postfix="device"
dev_file_type_set="dev_type"

phone_socket_dir="/dev/socket"
socket_type_postfix="socket"
file_type_set="file_type"

file_contexts_fobj=[]
file_contexts_flag=[]
file_contexts_label=[]
cnt_label=0

init_update=False

def del_after_char(line,spec_char):
    #
    # delete the input line after specific char (ex : . / -)
    #
    cmd_idx = line.find(spec_char)
    if cmd_idx != -1:
        line_valid = line[:cmd_idx]
    else:
        line_valid=line
    return line_valid

def split_by_whitespace(line):    
    #
    # split the input line by whitespace (used in read file)
    #
    line_split = filter(None,line.replace("\t"," ").replace("\n"," ").replace("\r"," ").split(" "))
    return line_split

def append_label(fobj,type_label,flag):
    #
    # append input contexts to file_context structure 
    #
    # input description
    # =======================
    # fobj, type_label, flag : file_context setting
    # =======================
    global cnt_label
    global file_contexts_fobj
    global file_contexts_flag
    global file_contexts_label
    #print "fobj = " + fobj
    #print "type_labe = " + type_label
    file_contexts_fobj.insert(cnt_label,fobj)
    file_contexts_flag.insert(cnt_label,flag)
    file_contexts_label.insert(cnt_label,type_label)  #"".join(["u:object_r:",domain,":s0"])
    cnt_label = cnt_label+1

#read and parsing orginal file_contexts
def read_file_contexts(fd):
    #
    # read original file_contexts setting
    #
    # input description
    # =======================
    # fd : file_contexts fd 
    # =======================
    global cnt_label
    global file_contexts_fobj
    global file_contexts_flag
    global file_contexts_label

    cnt_label = 0
    del file_contexts_fobj[:]
    del file_contexts_flag[:]
    del file_contexts_label[:]

    for line in fd:
        line_valid = del_after_char(line,"#")  #delete after #
        label = split_by_whitespace(line_valid)
        for cnt_item in range(len(label)):
            item = label[cnt_item]
            #check item
            if item.find("/") != -1:   #check the first fill is directory
                #bug?? (.*)? is not formal regular expression
                file_contexts_fobj.insert(cnt_label, item.replace("(.*)?",".*"))
                if item.find("(/.*)?") != -1:
                    file_contexts_flag.insert(cnt_label, "flag_all_under_dir")
                elif item is "/":
                    file_contexts_flag.insert(cnt_label, "flag_root_dir")
                else: 
                    file_contexts_flag.insert(cnt_label, "flag_matched_dir_file")
            elif item.find("--") != -1:
                file_contexts_flag.insert(cnt_label, "flag_matched_only_file")  #this two flag is exclusive
            elif item.find("u:") != -1:  #seandroid user is only 1 u:
                file_contexts_label.insert(cnt_label, item)
                cnt_label=cnt_label+1
                break
    for cnt_print in range(cnt_label):
        if debug: print print_tag + "file_contexts_label : " + file_contexts_fobj[cnt_print] + " " + file_contexts_flag[cnt_print] + " " + file_contexts_label[cnt_print] 
    if debug: 
        print print_tag + "Total Scan file_contexts number : " + str(cnt_label)
        print print_tag + "Scan file_contexts finished"


def find_dir(path,find_dir_name):
    #
    # find the specific dir path (ex : alps), for use in relative path.
    #
    # input description
    # =======================
    # path     :  the file path
    # dir_name :  the specific dir path name you want to find (ex : alps )
    # =======================
    while path != "":
        if find_dir_name == os.path.basename(path):
            return path
        else: 
            path = os.path.dirname(path)
        print "path is " + path
    print "Error in Path Dir in SEMTK_file_contexts_parser.py !!"
    print "There is no alps dir name"
    sys.exit(1);


def label_exec(out_exec_dir,fd,phone_dir_path,type_postfix,fd_log):
    #
    # label execution file and mark in the file_contexts
    #
    # input description
    # =======================
    # out_exect_dir   : scan dir relative path to out load_path 
    # fd              : file_contexts fd
    # phone_dir_path  : the exect directory path in the phone
    # type_postfix    : the executable file file type postfix , ex: _exec or _data_file or _binary
    # =======================
    exec_dir = "/".join([load_path,out_exec_dir])
    if debug: print print_tag + "Scan exec_dir : " + exec_dir
    for dirpath, dirnames, filenames in os.walk(exec_dir):
        for filename in filenames:
            check_file = "/".join([dirpath,filename])
            if debug: print "check_file : " + check_file
            if not os.path.islink(check_file): #skip link file
                # maybe in the dir ex: system/xbin/asan/app_process => asan_app_process
                domain_name = check_file.replace("/".join([exec_dir,""]),"")   
                if debug : print "domain_name : " + domain_name
                file_path = "/".join([phone_dir_path,domain_name])
                # for handling lost+found  
                filename_path = file_path.replace("+","\+")  
                if debug: print "filename_path : " + filename_path
                # search orinigal file_contexts
                if not match_file_contexts(filename_path,False):  #didn't set in original
                    type_label = filename_to_label(domain_name,False)
                    gen_te_file_true = gen_te_file(type_label,phone_dir_path,type_postfix)
                    if gen_te_file_true:
                        write_to_file_contexts(fd,file_path,type_label,type_postfix,"flag_matched_dir_file")
                        semtk_domain_new_file = "".join([type_label,".te"])
                        exec_source_file(fd_log,file_path,domain_name,semtk_domain_new_file)

def exec_source_file(fd_log,file_path,domain_name,semtk_domain_new_file):
    out_log_path = "".join(["out/target/product","/",product_name,"_android.log"])
    if debug: print "out_log_path : " + out_log_path
    if not find_source_by_target(fd_log,out_log_path,file_path,domain_name,semtk_domain_new_file):
        if not find_source_by_file(fd_log,out_log_path,file_path,domain_name,semtk_domain_new_file):
            fd_log.write("".join(["Error!! Cannot found source file of ",file_path," ",semtk_domain_new_file]))
            fd_log.write("\n")
    
    
def find_source_by_target(fd_log,out_log_path,file_path,type_label,semtk_domain_new_file):        
    found_source = False
    grep_pat = "".join(["target thumb C\+*: ",type_label, " <= "]) 
    grep_cmd_c = "".join(["grep -iE ","\"",grep_pat,"\" ",out_log_path])
    if debug: print "grep_cmd_c : " + grep_cmd_c
    cnt_cmd_c  = "| wc -l"
    is_exec_c = commands.getoutput(grep_cmd_c)
    cnt_exec_c = commands.getoutput("".join(["echo \"", is_exec_c," \"", cnt_cmd_c]))
    if debug: print "is_exec_c : " + is_exec_c
    if debug: print "cnt_exec_c : " + cnt_exec_c
    if is_exec_c != "":
        first_exec_c = del_after_char(is_exec_c,"\n")
        if debug: print "first_exec_c : " + first_exec_c
        if debug: print "grep_pat : " + grep_pat
        first_exec_c_split = split_by_whitespace(first_exec_c) 
        first_exec_source = first_exec_c_split[len(first_exec_c_split)-1]
        if debug: print "first_exec_source : " + first_exec_source
        fd_log.write(" ".join([file_path,first_exec_source,semtk_domain_new_file]))
        fd_log.write('\n')
        found_source = True
    return found_source

def find_source_by_file(fd_log,out_log_path,file_path,type_label,semtk_domain_new_file):
    found_source = False
    grep_pat = "".join(["[(Notice)|(Export includes)].*(file:.*"])  
    grep_cmd_c = "".join(["grep -iE ","\"",grep_pat,"\<",type_label,")\>\" ",out_log_path])
    if debug: print "grep_cmd_c : " + grep_cmd_c
    cnt_cmd_c  = "| wc -l"
    is_exec_c = commands.getoutput(grep_cmd_c)
    cnt_exec_c = commands.getoutput("".join(["echo \"", is_exec_c," \"", cnt_cmd_c]))
    if debug: print "is_exec_c : " + is_exec_c
    if debug: print "cnt_exec_c : " + cnt_exec_c
    if is_exec_c != "":
        first_exec_c = del_after_char(is_exec_c,"\n")
        first_exec_c_split = split_by_whitespace(first_exec_c) 
        first_exec_c_mod = first_exec_c_split[len(first_exec_c_split)-3]
        first_exec_source = first_exec_c_mod.replace("/NOTICE","/Android.mk")
        if debug: print "first_exec_source : " + first_exec_source
        if os.path.isfile(first_exec_source):
            fd_log.write(" ".join([file_path,first_exec_source,semtk_domain_new_file]))
            fd_log.write('\n')
            found_source = True
    return found_source

def filename_to_label(filename,is_init):
    #
    # according to the label naming rule modified the filename to label
    #
    # input description
    # =======================
    # filename      : filename
    # =======================
    #the type label is modified from filename
    if is_init: 
        type_label_noext = filename.replace(".","_")
    else:
        type_label_noext = del_after_char(filename,".")  #delete after .

    if re.match("[0-9].*",type_label_noext):  #can not number begin in the label
       type_label_nonum = "".join(["mtk_",type_label_noext])
    else:
       type_label_nonum = type_label_noext
    type_label = type_label_nonum.replace("/","_").replace("+","_").replace("-","_")
    return type_label

def write_to_file_contexts(fd,file_path,type_label,type_postfix,type_flag):
    #
    # create new label and write into file_contexts file/structure
    #
    # input description
    # =======================
    # fd             : add a new label in this fd file (file_contexts)
    # file_path      : the file path in the phone (for label setting)
    # type_label     : the type label
    # type_flag      : the new label append to file_context matrix, the type flag setting. 
    # =======================
    #special write new label into file_contexts file
    type_label = "".join(["u:object_r:",type_label,"_",type_postfix,":s0"])

    print "write a new label : " + "".join([file_path," ",type_label])
    fd.write("".join([file_path," ",type_label]))
    fd.write('\n')
    #special write new label into file_contexts structure
    append_label(file_path,type_label,type_flag)


def gen_te_file(domain,mode,type_postfix):                                  
    #
    # according to the label naming rule modified the filename to label
    #
    # input description
    # =======================
    # domain       : domain name ( /sbin/adbd : domain : adbd
    # mode         : the execution file is located in the which phone dir 
    # type_postfix : the execution file postfix (ex : exec)
    # =======================
    gen_te_file_true=False
    seandroid_domain_te_file="".join([alps_path,"/",seandroid_root_policy_dir,"/",domain,".te"])
    semtk_domain_product_te_file="".join([alps_path,"/",semtk_root_policy_dir,"/",domain,".te"])
    semtk_domain_new_file = "".join([semtk_policy_check_obj_dir,"/",domain,".te"])
    if debug: print "seandroid_domain_te_file = " + seandroid_domain_te_file
    if debug: print "semtk_domain_product_te_file = " + semtk_domain_product_te_file
    if debug: print "semtk_domain_new_file =" + semtk_domain_new_file
    #print seandroid_domain_te_file
    if not os.path.exists(seandroid_domain_te_file):
        if not os.path.exists(semtk_domain_product_te_file):
            if debug: print "create te file : " + semtk_domain_new_file
            fd=open(semtk_domain_new_file, 'w')
            fd.write("".join(["# automatic generate the basic policy file of ", domain, " executable file domain"]))
            fd.write('\n')
            fd.write("".join(["type ", domain,"_", type_postfix," , exec_type, file_type;"]))
            fd.write('\n')
            fd.write("".join(["type ", domain," ,domain;"]))
            fd.write('\n')
            if not mode==phone_sbin_dir:    
                fd.write("".join(["init_daemon_domain(", domain,")"]))
                fd.write('\n')
            fd.close()
            if debug: print "create te file Done : " + semtk_domain_product_te_file
            gen_te_file_true=True
    return gen_te_file_true                      


def label_init(socket_mode,out_init_dir,fd,phone_dir_path,type_postfix,fd_type,file_type_set):
    #
    # label /dev, /data,  socket important file/dir sets in the *.init.rc
    #
    # input description
    # =======================
    # out_init_dir    : scan *.init.rc path relative path to load_path 
    # fd              : file_contexts fd
    # phone_dir_path  : the exect directory path in the phone
    # type_postfix    : the executable file file type postfix , ex: _exec or _data_file or _binary
    # fd_type         : generate a new file using the fd_type (ex: "_dev.te" or "_data.te")
    # file_type_set   : file type setting (process domain is empty, but file structure is data or dev)
    # =======================

    global init_update

    init_dir="/".join([load_path,out_init_dir])

    if socket_mode:
        match_pat = ""
        search_file_pat = "\s+socket\s?"
    else:
        match_pat = "".join(["\\",phone_dir_path,"\\/"])
        search_file_pat = "".join(["(chmod|chown).*",match_pat])

    if debug: print "match_pat : " + match_pat
    for dirPath, dirNames, fileNames in os.walk(init_dir):  #find all file
        for fileName in fileNames:
            if fileName.find("init") != -1 and fileName.find(".rc") != -1:              #get *init.rc file
                file_path = "".join([dirPath,"/",fileName])
                if debug: print "init file_path : " + file_path
                for line in open(file_path, 'r'):           #search all line with chmod and chown
                    line_valid = del_after_char(line,"#")
                    if re.search(search_file_pat,line_valid): 

                        if socket_mode:
                            line_valid_mod = line_valid.replace("socket","")
                        else:
                            line_valid_mod = line_valid

                        if debug: print "matched line : " + line_valid_mod

                        label = split_by_whitespace(line_valid_mod)
                        for cnt_item in range(len(label)):
                            item = label[cnt_item]
                            match = re.match(match_pat,item)
                            if match or socket_mode :
                                if debug: print "init check item = " + str(item)

                                item_type_remove_root_dir = item.replace("".join([phone_dir_path,"/"]),"")
                                item_type = del_after_char(item_type_remove_root_dir,"/") #get the first hirarchy 

                                item_file_path = "".join([phone_dir_path,"/",item_type,"(/.*)?"])
                                item_file_valid = item_file_path.replace("+","\+")  #for handling lost+found 

                                if debug: print "item_type_remove_root_dir = " + item_type_remove_root_dir
                                if debug: print "item_file_valid = " + item_file_valid

                                if (item_type != "misc") and not match_file_contexts(item_file_valid,False):  #didn't set in original
                                    init_update=True
                                    type_label = filename_to_label(item_type,True)
                                    type_label_postfix = "".join([type_label,"_",type_postfix])
                                    if debug: print "type_label = " + type_label
                                    gen_type(fd_type,type_label_postfix,file_type_set) #check 
                                    write_to_file_contexts(fd,item_file_valid,type_label,type_postfix,"flag_all_under_dir")
                                break


def gen_type(fd_type,file_type,file_type_set):                                  
    #
    # check the device and file type is not already delcaration
    #
    # input description
    # =======================
    # fd_type         : the file needs to write the type into.
    # file_type       : find the file_type is exist or not 
    # file_type_set   : the exect directory path in the phone
    # =======================
    grep_cmd="".join(["grep ", file_type, " -il "])
    if debug: print "grep_cmd = " + "".join([grep_cmd,alps_path,"/",seandroid_root_policy_dir,"/*.te"])
    if debug: print "grep_cmd = " + "".join([grep_cmd,alps_path,"/",semtk_root_policy_dir,"/*.te"])
    is_seandroid_file_type = commands.getoutput("".join([grep_cmd,alps_path,"/",seandroid_root_policy_dir,"/*.te"]))
    is_semtk_product_file_type = commands.getoutput("".join([grep_cmd,alps_path,"/",semtk_root_policy_dir,"/*.te"]))
    if debug: print "seandroid has the type? " + is_seandroid_file_type 
    if debug: print "semtk has the type? " + is_semtk_product_file_type 
    #print seandroid_domain_te_file
    if is_seandroid_file_type == "" and is_semtk_product_file_type == "": 
        if debug: print file_type + "into " + "fd_type"
        fd_type.write("".join(["type ", file_type,", ", file_type_set, ";"]))
        fd_type.write('\n')
    else: 
        print "the type is already exist : " + file_type + " in " + is_seandroid_file_type + is_semtk_product_file_type

def match_file_contexts(file_path,all_under_dir_valid):
    #
    # find the file_path is already in the file_contexts or not
    #
    # input description
    # =======================
    # file_path            : the search file path
    # all_under_dir_valid  : search about dir (./*)  
    # =======================
    global cnt_label
    global file_contexts_fobj
    global file_contexts_flag
    global file_contexts_label

    match = False

    for cnt_scan in range(cnt_label):
        # XXX/YYY/(./*) match ignore 
        match_all_under_dir = all_under_dir_valid and file_contexts_flag[cnt_scan].find("flag_all_under_dir")!=-1 #found
        # the file-context setting is /  => root setting => ignore
        match_root_dir = file_contexts_flag[cnt_scan].find("flag_root_dir")!=-1                                   #found
        # exact file assign in original in file_contexts
        match_exact_dir = file_path == file_contexts_fobj[cnt_scan]

        if match_exact_dir:
            match = True
            break
        elif  match_all_under_dir and not match_root_dir:  #ignore all_under_dir -1:not match
            match = re.search("".join(["^",file_contexts_fobj[cnt_scan],"$"]),file_path)   #match string begin  1:pat 2:search string
            if (match) or (match_exact_dir):
                match = True
                break
    if debug: print "match original file_contexts?" + str(match)
    if debug and match: print "org : " + file_contexts_fobj[cnt_scan] + "  new : " + file_path
    return match


def main():

    global out_dir
    global semtk_policy_check_obj_dir
    global semtk_root_policy_dir
    global product_name
    global alps_path
    global load_path

    if len(sys.argv) == 2:
        product_in=sys.argv[1]
        out_dir = os.environ.get('OUT_DIR')
    elif len(sys.argv) == 3: 
        product_in=sys.argv[1]
        out_dir=sys.argv[2]
    else: 
        print print_tag + "Error in Usage SEMTK_policy_check.py !!"
        print sys.argv[0] + "  [product_name]"
        print sys.argv[0] + "  [product_name] + [output diretory]"
        sys.exit(1);
    if debug: print product_in
    if debug: print out_dir

    #set alps path
    product_name = del_after_char(product_in,"[")
    alps_path = find_dir(os.getcwd(),"alps");
    load_path = "/".join([out_dir,product_name])

    #set android policy path
    seandroid_root_policy_dir    = "external/sepolicy"
    seandroid_file_contexts_file = "/".join([alps_path,seandroid_root_policy_dir,semtk_file_contexts_filename]) 

    #set mtk policy path
    semtk_root_policy_dir    = "mediatek/custom/out/" + product_in + "/sepolicy"
    semtk_file_contexts_file = "/".join([alps_path,semtk_root_policy_dir,semtk_file_contexts_filename])

    if debug: print print_tag + seandroid_file_contexts_file
    if debug: print print_tag + semtk_file_contexts_file

    #make policy directory
    semtk_policy_check_obj_dir = "/".join([load_path,"obj",semtk_policy_check_dir])
    if not os.path.exists(semtk_policy_check_obj_dir):
        os.mkdir(semtk_policy_check_obj_dir)
    else:
        shutil.rmtree(semtk_policy_check_obj_dir)
        os.mkdir(semtk_policy_check_obj_dir)

    #open policy log file
    semtk_policy_log_file = "".join([out_dir,"/",product_name,"_",semtk_policy_log_filename])
    if debug: print print_tag + semtk_policy_log_file
    fd_log=open(semtk_policy_log_file, 'w')

    semtk_dev_type_file  = "/".join([alps_path,semtk_root_policy_dir,semtk_dev_type_filename])
    semtk_file_type_file = "/".join([alps_path,semtk_root_policy_dir,semtk_file_type_filename])

    semtk_file_contexts_new_file = "/".join([semtk_policy_check_obj_dir,semtk_file_contexts_filename])
    semtk_dev_type_new_file  = "/".join([semtk_policy_check_obj_dir,semtk_dev_type_filename])
    semtk_file_type_new_file = "/".join([semtk_policy_check_obj_dir,semtk_file_type_filename])

    #open default file_contexts to check
    if os.path.exists(semtk_file_contexts_file):
        shutil.copyfile(semtk_file_contexts_file,semtk_file_contexts_new_file)
        fd_fc=open(semtk_file_contexts_new_file, 'r+w')
    else:
        shutil.copyfile(seandroid_file_contexts_file,semtk_file_contexts_new_file)
        fd_fc=open(semtk_file_contexts_new_file, 'r+w')

    if os.path.exists(semtk_dev_type_file):
        shutil.copyfile(semtk_dev_type_file,semtk_dev_type_new_file)
    fd_dev=open(semtk_dev_type_new_file, 'a')

    if os.path.exists(semtk_file_type_file):
        shutil.copyfile(semtk_file_type_file,semtk_file_type_new_file)
    fd_file=open(semtk_file_type_new_file, 'a')

    if debug: print print_tag + semtk_policy_log_file
    if debug: print print_tag + semtk_file_contexts_new_file
    if debug: print print_tag + semtk_dev_type_new_file
    if debug: print print_tag + semtk_file_type_new_file
    
    #fd_fc move to the start of the file
    fd_fc.seek(0)
    read_file_contexts(fd_fc)

    #fd_fc move to end of the file
    fd_fc.seek(0,os.SEEK_END)
    
    label_exec(out_sbin_path,fd_fc,phone_sbin_dir,exec_type_postfix,fd_log)
    label_exec(out_system_bin_path,fd_fc,phone_system_bin_dir,exec_type_postfix,fd_log)
    label_exec(out_system_xbin_path,fd_fc,phone_system_xbin_dir,exec_type_postfix,fd_log)
    label_exec(out_vendor_bin_path,fd_fc,phone_vendor_bin_dir,exec_type_postfix,fd_log)
    label_exec(out_vendor_xbin_path,fd_fc,phone_vendor_xbin_dir,exec_type_postfix,fd_log)

    label_init(False, out_init_dir,fd_fc,phone_data_dir,data_type_postfix,fd_file,data_file_type_set)
    label_init(False, out_init_dir,fd_fc,phone_data_misc_dir,data_type_postfix,fd_file,data_file_type_set)
    label_init(False, out_init_dir,fd_fc,phone_dev_dir,dev_type_postfix,fd_dev,dev_file_type_set)
    label_init(True,  out_init_dir,fd_fc,phone_socket_dir,socket_type_postfix,fd_dev,file_type_set)

    if init_update:
        fd_log.write("".join(["file_contexts mediatek/custom/",product_in,"/sepolicy/file_contexts"]))
        fd_log.write("\n")

    fd_fc.close()
    fd_dev.close()
    fd_file.close()
    fd_log.close()

if __name__ == "__main__":
    main()
