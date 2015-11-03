/*
 * JavaScript Library
 * Copyright (c) 2015 mxchip.com
 */
(function(window) {
	var m = {};
	m.test = function(str) {
		return str.length;
	};

	//获取设备列表
	m.getDevList = function(userToken, callback) {
		var sucm;
		var errm;
		AV.Cloud.run('getDevList', {
			userToken : userToken,
		}, {
			success : function(ret) {
				callback(ret.data, errm);
			},
			error : function(err) {
				//处理调用失败
				callback(sucm, err);
			}
		});
	};

	//获取owner权限的设备
	m.getAuthDev = function(userToken, callback) {
		var sucm;
		var errm;
		AV.Cloud.run('getAuthDev', {
			userToken : userToken,
		}, {
			success : function(ret) {
				callback(ret.data, errm);
			},
			error : function(err) {
				//处理调用失败
				callback(sucm, err);
			}
		});
	};

	//修改设备名称
	m.editDevName = function(appid, usertoken, devname, devid, callback) {
		var sucm;
		var errm;
		$.ajax({
			url : "http://api.easylink.io/v1/device/modify",
			type : 'POST',
			data : JSON.stringify({
				device_id : devid,
				alias : devname
			}),
			headers : {
				"Content-Type" : "application/json",
				"X-Application-Id" : appid,
				"Authorization" : "token " + usertoken
			},
			success : function(ret) {
				callback("success", errm);
			},
			error : function(err) {
				callback(sucm, err);
			}
		});
	};

	//列出设备下所有用户
	m.devUserQuery = function(appid, usertoken, devid, callback) {
		var sucm;
		var errm;
		$.ajax({
			url : "http://www.easylink.io/v1/device/user/query",
			type : 'POST',
			data : JSON.stringify({
				device_id : devid
			}),
			headers : {
				"Content-Type" : "application/json",
				"X-Application-Id" : appid,
				"Authorization" : "token " + usertoken
			},
			success : function(ret) {
				callback(ret, errm);
			},
			error : function(err) {
				callback(sucm, err);
			}
		});
	};

	//删除设备下的某个用户
	m.deleteOneUser = function(appid,usertoken, userid, devid, callback) {
		var sucm;
		var errm;
		$.ajax({
			url : "http://www.easylink.io/v1/device/user/delete",
			type : 'POST',
			data : JSON.stringify({
				device_id : devid,
				user_id : userid
			}),
			headers : {
				"Content-Type" : "application/json",
				"X-Application-Id" : appid,
				"Authorization" : "token " + usertoken
			},
			success : function(ret) {
				callback(ret, errm);
			},
			error : function(err) {
				callback(sucm, err);
			}
		});
	};

	//删除设备
	m.deleteDev = function(appid, usertoken, devid, callback) {
		var sucm;
		var errm;
		$.ajax({
			url : "http://api.easylink.io/v1/device/delete",
			type : 'POST',
			data : JSON.stringify({
				device_id : devid
			}),
			headers : {
				"Content-Type" : "application/json",
				"X-Application-Id" : appid,
				"Authorization" : "token " + usertoken
			},
			success : function(ret) {
				callback("success", errm);
			},
			error : function(err) {
				callback(sucm, err);
			}
		});
	};

	//获取设备id
	m.getDevid = function(devip, devpsw, devtoken, callback) {
		var sucm;
		var errm;
		var ajaxurl = 'http://' + devip + ':8001/dev-activate';
		$.ajax({
			url : ajaxurl,
			type : 'POST',
			data : JSON.stringify({
				login_id : "admin",
				dev_passwd : devpsw,
				user_token : devtoken
			}),
			headers : {
				"Content-Type" : "application/json"
			},
			success : function(ret) {
				callback(ret, errm);
			},
			error : function(err) {
				callback(sucm, err);
			}
		});
	};

	//获取设备激活状态
	m.getDevState = function(devip, devtoken, callback) {
		var sucm;
		var errm;
		var ajaxurl = 'http://' + devip + ':8001/dev-state';
		$.ajax({
			url : ajaxurl,
			type : 'POST',
			data : JSON.stringify({
				login_id : "admin",
				dev_passwd : "88888888",
				user_token : devtoken
			}),
			headers : {
				"Content-Type" : "application/json"
			},
			success : function(ret) {
				callback(ret, errm);
			},
			error : function(err) {
				callback(sucm, err);
			}
		});
	};

	//去云端绑定设备
	m.bindDevCloud = function(appid, usertoken, devtoken, callback) {
		var sucm;
		var errm;
		$.ajax({
			url : 'http://api.easylink.io/v1/key/authorize',
			type : 'POST',
			data : {
				active_token : devtoken
			},
			headers : {
				"Authorization" : "token " + usertoken,
				"X-Application-Id" : appid
			},
			success : function(ret) {
				callback(ret, errm);
			},
			error : function(err) {
				callback(sucm, err);
			}
		});
	};

	//授权设备
	m.authDev = function(appid, usertoken, phone, devid, callback) {
		var sucm;
		var errm;
		$.ajax({
			url : 'http://api.easylink.io/v1/key/user/authorize',
			type : 'POST',
			data : {
				login_id : phone,
				owner_type : "share",
				id : devid,
			},
			headers : {
				"Authorization" : "token " + usertoken,
				"X-Application-Id" : appid
			},
			success : function(ret) {
				callback(ret, errm);
			},
			error : function(err) {
				callback(sucm, err);
			}
		});
	};

	/*end*/
	window.$mico = m;

})(window);
