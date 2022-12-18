
var config_json=null;
var config_json_copy=null;
const config_file_name ="/config/config.json";
function config_on_load(){
    SendGetHttp(config_file_name, process_answer,on_ResponceErrorLog);
}

function config_changed(val){
    $('#id_save').prop('disabled', !val);
    $('#id_cancel').prop('disabled', !val);
}

function process_answer(response_text) {
    try { 
        var response = JSON.parse(response_text);
        console.log(response);
        config_json = response;
        config_json_copy= {...response};
        create_items();
        config_changed(false);
    } catch (e) {
        console.error("Parsing error:", e);
    }
}

function create_property(key, value){
    console.log(key, value);
    const input_type=Number.isInteger(value)?"number":"text";
    const input_val=Number.isInteger(value)?"Number(this.value)":"this.value";
    var content  ='<div class="input-group">';
    content += `<span class="input-group-text col-3" id="id_s_${key}">${key}</span>`
    content += `<input type="${input_type}" class="form-control"  id="id_i_${key}" aria-describedby="id_s_${key}" value="${value}" onchange="config_edit('${key}',${input_val})">`
    content +="</div>"
    return content;
}
function create_items(){
    var content=""
    Object.entries(config_json).forEach(function(item){
        content+=create_property(item[0],item[1]);
    });
    $('#id_config').html( content);
}

function config_edit(key,val){
    console.log(key)
    config_json[key]=val;
    config_changed(true);

}

function config_save(){
    config_json_copy={...config_json};
    url="/filesave?path="+config_file_name+"&payload="+encodeURIComponent(JSON.stringify(config_json));
    SendGetHttp(url,undefined,on_ResponceErrorLog);
    config_edit_exit();
}

function config_cancel(){
    config_json={...config_json_copy};
    config_edit_exit();
}

function config_edit_exit(){
    config_changed(false);
    create_items();
}



