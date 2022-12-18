function preset_cmd_on_load(){
    preset_init("/config/preset_cmd.json");
}

//-------------- preset
function preset_button_caption(item){
    return '<strong>'+ item.cmd+ "</strong> ("+item.handler+":"+item.val  +")";
}

function preset_dialog_get(){
    return {cmd:$('#id_edit_item_dialog_name').val(),handler:$('#id_edit_item_handler').val(),val:$('#id_edit_item_val').val()}
}

function preset_isDublicate(lhs,rhs){
    return lhs.cmd===rhs.cmd;
}

function preset_send(item){
    console.log(item); 
    url="/command?handler="+item.handler;
    url+="&val="+item.val;
    SendGetHttp(url,undefined,on_ResponceErrorLog);
}

function present_edit_onOpen(index,item){
    if(-1!==edit_index){
        $('#id_edit_item_dialog_head').html("edit");
        $('#id_edit_item_dialog_name').val(item.cmd);
        $("#id_edit_item_handler").val(item.handler);
        $('#id_edit_item_val').val(item.val);
    }else{
        $('#id_edit_item_dialog_head').html("Add");
    }
}
//<------




