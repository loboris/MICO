/**
 * Created by rocke on 15-7-22.
 */
var userToken;
var micobindobj;
//系统版本标记
var sysverid = 0;
//mdns的对象
var micoMmdns;
//是否提示找到设备
var saytag = 0;
var dev_token;
//临时存储wifi名字
var wifiNameTmp = "";


//获取ssid
function getssid() {
	var wifissid = api.require('wifiSsid');
	wifissid.getSsid(function(ret, err) {
		if (ret.ssid) {
			$("#wifi_ssid").val(ret.ssid);
			if (wifiNameTmp != ret.ssid) {
				$("#wifi_psw").val("");
			}
			wifiNameTmp = ret.ssid;
		} else {
			api.alert({
				msg : err.msg
			});
		}
	});
}

//开始配网
function startmicobind() {
	searchtag = 1;
	showProgress(CONNECT_NET, false);
	micobindobj = api.require('micoBind');
	var ssid = $("#wifi_ssid").val();
	var psw = $("#wifi_psw").val();
	micobindobj.getDevip({
		wifi_ssid : ssid,
		wifi_password : psw
	}, function(ret, err) {
	});
}

//停止配网
function stopmicobind() {
	micobindobj.stopFtc(function(ret, err) {
	});
}

//获取mdns设备列表
function getmDNSlist() {
	micoMmdns = api.require("micoMdns");
	var serviceType = "_easylink._tcp";
	var inDomain = "local";
	saytag = 0;
	micoMmdns.startMdns({
		serviceType : serviceType,
		inDomain : inDomain
	}, function(ret, err) {
		var html = "";
		var sbtag = 0;
		if (ret.status) {
			$.each(ret.status, function(n, value) {
				if (("false" == value.deviceMacbind)) {
					sbtag = 1;
					html += '<li class="mdnsdevcls" onclick="ajaxgetdveid(\'' + value.deviceIP + '\')"><a><img src="../image/jihuoshebei.svg"/><div><p class="mdnsdjhsbname">' + value.deviceName + '</p><p class="mdnsdjhsbip">IP：' + value.deviceIP + '</p></div></a></li>';
				}
			});
			if (1 == sbtag) {
				$(".mdnsdjhsb").css("display", "block");
				if (0 == saytag) {
					saytag = 1;
					findUnactiDev();
				}
			}
			$("#mdnsdevliid").html(html);
		}
	});
}

//提示找到设备
function findUnactiDev() {
	if (1 == saytag) {
		apiToast(FIND_UNACTIV_DEV, 5000);
		saytag = 2;
		if (1 == searchtag) {
			hidPro();
			api.confirm({
				title : "去绑定已发现的设备",
				msg : "爱谁，点谁",
				buttons : [OK_BTN, CANCEL_BTN]
			}, function(ret, err) {
				if (ret.buttonIndex == 1) {
					searchtag = 0;
					stopmicobind();
				} else {
					showProgress(CONNECT_NET, false);
				}
			});
		}
	}
}

//停止扫描
function stopMdns() {
	micoMmdns.stopMdns(function(ret, err) {
	});
}

//绑定设备
function ajaxgetdveid(devip) {
	api.confirm({
		title : ACTIVATE_DEV,
		msg : SURE_ACTIV_DEV,
		buttons : [OK_BTN, CANCEL_BTN]
	}, function(ret, err) {
		if (ret.buttonIndex == 1) {
			showProgress(SET_DEV_PSW, false);
			userToken = getUserInfo().get("userToken");
			dev_token = $.md5(devip + userToken);
			var dev_psw = "1234";
			$mico.getDevid(devip, dev_psw, dev_token, function(ret, err) {
				if (ret) {
					var devid = ret.device_id;
					bindtocloud(devid);
				} else {
					$("#backleft").css("display", "block");
					hidPro();
					apiToast(W_AND_TRY, 2000);
				}
			});
		}
	});
}

//去云端绑定设备
function bindtocloud(devid) {
	$mico.bindDevCloud(APP_ID, userToken, dev_token, function(ret, err) {
		if (ret) {
			//页面跳转
			apiToast(ACTIVATE_SUCC, 2000);
			hidPro();
		} else {
			$("#backleft").css("display", "block");
			hidPro();
			alert(JSON.stringify(err));
		}
	});
}
