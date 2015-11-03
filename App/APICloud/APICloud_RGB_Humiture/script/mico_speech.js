/**
 *2015-08-03 by Rocke
 */

function speechCtrl(msg) {
	var command;
	if (msg.indexOf("灯") > -1 || (msg.indexOf("机") > -1) || (msg.indexOf("led") > -1)|| (msg.indexOf("设备") > -1)) {
		if (msg.indexOf("开") > -1) {
			command = '{"device_switch":true}';
		} else if (msg.indexOf("关") > -1) {
			command = '{"device_switch":false}';
		} else if (msg.indexOf("红") > -1) {
			command = '{"rgbled_switch":true,"rgbled_hues":0, "rgbled_saturation":100, "rgbled_brightness":100}';
		} else if (msg.indexOf("黄") > -1) {
			command = '{"rgbled_switch":true,"rgbled_hues":60, "rgbled_saturation":100, "rgbled_brightness":100}';
		} else if (msg.indexOf("绿") > -1) {
			command = '{"rgbled_switch":true,"rgbled_hues":120, "rgbled_saturation":100, "rgbled_brightness":100}';
		} else if (msg.indexOf("调") > -1) {
			var hues = msg.replace(/[^0-9]/ig,"");
			if (hues > -1) {
				command = '{"rgbled_switch":true,"rgbled_hues":'+ hues +', "rgbled_saturation":100, "rgbled_brightness":100}';
			}
		}
	}
	if(msg.indexOf("bye") > -1 || (msg.indexOf("再见") > -1)){
		command = '{"device_switch":false}';
	}
	return command;
}