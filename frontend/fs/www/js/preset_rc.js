var cmd_list=null;

function preset_rc_on_load(){
    preset_init("/config/preset_rc.json");
    SendGetHttp("/config/preset_cmd.json", process_preset_cmd_answer,on_ResponceErrorLog);
}
//-------------- preset
function preset_button_caption(item){
    return '<strong>'+ item.code+ "</strong> ("+item.cmd+")";
}

function preset_dialog_get(){
    return {code:Number($('#id_edit_item_dialog_rc').text()),cmd:$('#id_edit_item_cmd').val()}
}

function preset_isDublicate(lhs,rhs){
    console.log('not implemented');
}

function preset_send(item){
    console.log(item); 
    const found = cmd_list.find(element => element.name === item.cmd);
    console.log(found);
    if(found){
        url="/command?handler="+found.handler;
        url+="&val="+found.val;
        SendGetHttp(url,undefined,on_ResponceErrorLog);
    }
}

function present_edit_onOpen(index,item){
    if(-1!==edit_index){
        $('#id_edit_item_dialog_rc').text(item.code);
        $('#id_edit_item_cmd').val(item.cmd);
        $('#id_edit_item_dialog_head').html("edit");
    }else{
        $('#id_edit_item_dialog_head').html("Add");
    }
}
//<------

function process_preset_cmd_answer(response_text) {
    try { 
        var response = JSON.parse(response_text);
        console.log(response);
        cmd_list=response.items;
        cmd_list.forEach(function(item){
          $('#id_edit_item_cmd').append(`<option value="${item.cmd}"> ${item.cmd}</option>`);  
        })
    } catch (e) {
        console.error("Parsing error:", e);
    }
}

function preset_rc_scan(){
    $('#id_edit_item_dialog_rc').text("wait IR");
    var url = "/get_rc_val";
    SendGetHttp(url, preset_on_rc_scan1,function(res){
        console.error(res);
        $('#id_edit_item_dialog_rc').text("fail");
    });
}

var rc_val=0;
function preset_on_rc_scan1(response){
    console.log(response);
    $('#id_edit_item_dialog_rc').text("one more");
    rc_val=Number(JSON.parse(response).rc_val);
    var url = "/get_rc_val";
    SendGetHttp(url, preset_on_rc_scan2,function(res){
        console.error(res);
        $('#id_edit_item_dialog_rc').text("fail");
    });
}

function preset_on_rc_scan2(response){
    console.log(response);
    if(rc_val!==Number(JSON.parse(response).rc_val)){
         $('#id_edit_item_dialog_rc').text("different codes");
    }else{
        $('#id_edit_item_dialog_rc').text(rc_val);
    }
}

