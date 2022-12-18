function Monitor_comment(str){
    //console.log("answer:" + str);
}

function SendGetHttp(url, resultfn, errorfn) {
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function() {
        if (xmlhttp.readyState == 4) {
            if (xmlhttp.status == 200) {                
                Monitor_comment(xmlhttp.responseText);
                if (typeof resultfn != 'undefined' && resultfn != null) resultfn(xmlhttp.responseText);
            } else {                
                if (typeof errorfn != 'undefined' && errorfn != null) errorfn(xmlhttp.status, xmlhttp.responseText);
            }
        }
    }    
//    console.log("GET:" + url);

    xmlhttp.open("GET", url, true);
    xmlhttp.send();
}

function http_init_place_holder_fromfile(id,file,funcAfter){
    document.getElementById(id).innerHTML="";
    var rawFile = new XMLHttpRequest();
    rawFile.open("GET", file);    
    rawFile.onreadystatechange = function ()
    {
        if(rawFile.readyState === 4)
        {
            if(rawFile.status === 200 || rawFile.status == 0)
            {
                document.getElementById(id).innerHTML = rawFile.responseText;
                if (typeof funcAfter != 'undefined' && funcAfter != null){
                    setTimeout(funcAfter,100)
                }  
            }
        }
    }
    rawFile.send(null);
}


function SendFileHttp(url, postdata, progressfn,resultfn, errorfn){
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function() {
        if (xmlhttp.readyState == 4) {
            http_communication_locked = false;
            if (xmlhttp.status == 200 ){
                if (typeof resultfn != 'undefined' && resultfn != null )resultfn(xmlhttp.responseText);
            }
            else {
                if (xmlhttp.status == 401)GetIdentificationStatus();
                if (typeof errorfn != 'undefined' && errorfn != null)errorfn(xmlhttp.status, xmlhttp.responseText);
            }
        }
    }
    const files=postdata.getAll('myfile[]');
    Monitor_comment("POST:"+ url+" l:"+files.length +" [0]:"+files[0].name);
    xmlhttp.open("POST", url, true);
    if (typeof progressfn !='undefined' && progressfn != null)xmlhttp.upload.addEventListener("progress", progressfn, false);
    xmlhttp.send(postdata);
}

function GetFileHttp(url, resultfn, errorfn) {
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function() {
        if (xmlhttp.readyState == 4) {
            if (xmlhttp.status == 200) {                
                Monitor_comment(xmlhttp.responseText);
                if (typeof resultfn != 'undefined' && resultfn != null) resultfn(xmlhttp.responseText);
            } else {                
                if (typeof errorfn != 'undefined' && errorfn != null) errorfn(xmlhttp.status, xmlhttp.responseText);
            }
        }
    }    
//    console.log("GET:" + url);
    Monitor_comment("GET:" + url);
    xmlhttp.open("GET", url, true);
    xmlhttp.send();
}

function on_ResponceErrorLog(responce){
    console.log("Error "+responce);
}
