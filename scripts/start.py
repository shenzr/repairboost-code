import os
import subprocess

filepath = os.path.realpath(__file__)
script_dir = os.path.dirname(os.path.normpath(filepath))
home_dir = os.path.dirname(os.path.normpath(script_dir))
conf_dir = home_dir+"/conf"
meta_dir = home_dir+"/meta"
CONF = conf_dir+"/config.xml"
# print(CONF)
# read from configuration file of the slaves

f = open(CONF)
start = False
concactstr = ""
for line in f:
    if line.find("setting") == -1:
        line = line[:-1]
        concactstr += line
res = concactstr.split("<attribute>")

slavelist = []
metaStripeDir = ""
for attr in res:
    if attr.find("helpers.address") != -1:
        valuestart = attr.find("<value>")
        valueend = attr.find("</attribute>")
        attrtmp = attr[valuestart:valueend]
        slavestmp = attrtmp.split("<value>")
        for slaveentry in slavestmp:
            if slaveentry.find("</value>") != -1:
                entrysplit = slaveentry.split("/")
                slaveentry = entrysplit[1]
                endpoint = slaveentry.find("</value>")
                slave = slaveentry[:endpoint]
                slavelist.append(slave)
    elif attr.find("meta.stripe.dir") != -1:
        valuestart = attr.find("<value>")
        valueend = attr.find("</value>")
        attrtmp = attr[valuestart:valueend]
        metaStripeDir = str(attrtmp.split("<value>")[1])

# start
print("start coordinator")
os.system("redis-cli flushall")
os.system("killall ECCoordinator")
command = "cd "+home_dir+"; ./ECCoordinator &> "+home_dir+"/output &"
print(command)
subprocess.Popen(['/bin/bash', '-c', command])


for slave in slavelist:
    print("start slave on " + slave)
    os.system("ssh " + slave + " \"killall ECHelper \"")
    os.system("ssh " + slave + " \"killall ECClient \"")
    
    # -- for standalone-system
    # command = "rm -r " + metaStripeDir + "; "
    # command += "mkdir " + metaStripeDir + "; "
    # command = "ssh " + slave + " \""+ command +  "\""
    # os.system(command)

    command = "scp "+home_dir+"/ECHelper "+slave+":"+home_dir+"/"
    os.system(command)

    command = "scp "+home_dir+"/ECClient "+slave+":"+home_dir+"/"
    os.system(command)
    os.system("ssh " + slave + " \"redis-cli flushall \"")

    command = "ssh "+slave+" \"cd "+home_dir + \
        "; ./ECHelper &> "+home_dir+"/output &\""
    print(command)
    subprocess.Popen(['/bin/bash', '-c', command])
