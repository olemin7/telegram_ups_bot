#!/usr/bin/python3
#ver 1-dec-2019
import os
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse
from urllib.parse import parse_qs
from urllib.parse import urlencode
from urllib.parse import unquote
import json
import argparse

fileExt={
    "htm"   :"text/html",
    "html"  :"text/html",
    "xml"   :"text/xml",
    "js"    :"application/javascript",
    "css"   :"text/css",
    "json"  :"application/json",
    "txt"   :"text/plain",
    "ico"   :"image/x-icon",
    "gz"    :"application/zip"
}

class Route(object):    
    source_type = ""    
    data_item =""
    content_type = ""
    def __init__(self, source_type,data_item, content_type):
        self.content_type = content_type
        self.source_type = source_type
        self.data_item = data_item
    def toString(self):        
        return "type="+self.source_type+",item="+self.data_item+", content_type="+self.content_type

handler ={}
def printHandlers():
    print("Handler list:");
    for rule in handler:
        if handler[rule] == None:
            print ("'" + rule + "'<None>")
        else:
            print ("'" + rule + "'<" + handler[rule].toString() + ">")

workPatch=""
# HTTPRequestHandler class
class testHTTPServer_RequestHandler(BaseHTTPRequestHandler):
    def send_Error(self,error):
        print('return Error:'+ str(error))
        self.send_response(error)
        self.send_header("Content-type", "text/plain")
        self.end_headers() 
    def do_File(self,filename,content_type):        
        try:
            fo = open(filename, "rb")
        except:
            # doesn't exist            
            print('File '+filename+' NotFoundError')            
            self.send_Error(404)
        else:
            self.send_response(200)
            self.send_header("Content-type", content_type)
            if os.path.splitext(filename)[1][1:]=="gz":
                self.send_header("Content-Encoding", "gzip")
            self.end_headers()
            self.wfile.write(fo.read())
            fo.close()
        return

    def do_Reply(self):
        uir=unquote(self.path)
        print(uir)
        o = urlparse(self.path)
        print(o)
        print(parse_qs(o.query))        
        #uir = o.path        
        if uir in handler:
            rule=handler[uir];
            if rule == None:#stub
                self.do_HEAD()
                return
            print("rule="+rule.toString())        
            if rule.source_type == "file":
                self.do_File(rule.data_item, rule.content_type) 
                return;
            if rule.source_type == "inline":
                self.send_response(200)
                self.send_header("Content-type", rule.content_type)
                self.end_headers()            
                self.wfile.write(bytes(rule.data_item, "utf8"))
                return
        # hande as file
        f_isfile=os.path.isfile(wwwSourceDir+ uir);
        f_isfile_gz=os.path.isfile(wwwSourceDir+ uir+".gz");
        print(f_isfile) 
        print(f_isfile_gz) 
        if (f_isfile or f_isfile_gz):
            extension = os.path.splitext(uir)[1][1:]
            try:
                content_type = fileExt[extension]
            except:
                print(' unknowFileTypeERROR')
                self.send_Error(404)
                return
            if f_isfile_gz:
                self.do_File(wwwSourceDir+ uir+".gz",content_type)
            else:
                self.do_File(wwwSourceDir+ uir,content_type)
            return

        print('unknow handler')
        self.send_Error(404)
        return        
    def do_HEAD(s):
        s.send_response(200)
        s.send_header("Content-type", "text/html")
        s.end_headers() 
    def do_GET(self):
        self.do_Reply()
    def do_PUT(self):
        self.do_Reply()

def run(PORT):
    print('starting server...')
    server_address = ('127.0.0.1', PORT)
    httpd = HTTPServer(server_address, testHTTPServer_RequestHandler)
    printHandlers()

    print('running server... ')
    print('http://localhost:',PORT,sep="")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()


parser = argparse.ArgumentParser(description='no')
parser.add_argument('-c', dest='configFile', help='patch to config File',  required=True)
parser.add_argument('-p', dest='port',type=int,default=8081, help='server port', )
args = parser.parse_args()
configDir ="./" #cur dir
if os.path.dirname(args.configFile) !="":
    configDir=os.path.dirname(args.configFile)+"/"
print("configDir="+configDir)
print("parce config file" )
with open(args.configFile) as data_file:
    data_item = json.load(data_file)

#add rules from wwwSourceDir
wwwSourceDir=configDir+data_item["wwwSourceDir"]+"/"
ignoredFiles=[];
print("wwwSourceDir="+wwwSourceDir)

#add rules from json (rewrite wwwSourceDir)
for rule in data_item["handler"]:
    if "entry" not in rule:
        continue
    if "file" in rule:
        filename=os.path.normpath(configDir+rule["file"]);
        if "type" in rule:
            content_type=rule["type"]
        else:
            extension = os.path.splitext(filename)[1][1:]
            try:
                content_type = fileExt[extension]
            except:
                print(' unknowFileTypeERROR')
                exit(1)    
        handler.update({rule["entry"]: Route("file", filename,content_type)});
        continue
    if "inline" in rule:
        if "type" not in rule:
            print(' unknowContentTypeERROR')
            exit(1)    
        handler.update({rule["entry"]: Route("inline", rule["inline"],rule["type"])});
        continue
    handler.update({rule["entry"]:None}); #no info


#printHandlers()
run(args.port)

