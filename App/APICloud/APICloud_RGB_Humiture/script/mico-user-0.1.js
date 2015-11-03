/*
 * User Management JavaScript Library
 * Copyright (c) 2015 mxchip.com
 */
(function(window) {
	AV.$ = jQuery;

	//用config.js中的appId，appKey初始化，若你搭建自己的js，请用你的appId与appKey
	AV.initialize(appId, appKey);
	var currentUser = AV.User.current();
	var m = {};

	function sucRet() {
		var b = '{"code":"' + errcode("") + '","msg":"没找到"}';
		var bToObj = JSON.parse(b);
		return bToObj;
	}

	function errcode(sit) {
		switch(sit) {
			case "":
				return 1;
		}
	}

	function errRet(code, msg) {
		var b = '{"code":"' + errcode("") + '","msg":"' + msg + '"}';
		var bToObj = JSON.parse(b);
		return bToObj;
	}

	//获取登录信息
	m.islogin = function() {
		var currentUser = AV.User.current();
		return currentUser;
	};

	//注册用户
	m.loginWithPhone = function(phone, psw, callback) {
		var sucm;
		var errm;
		AV.User.logInWithMobilePhone(phone, psw).then(function(user) {
			//登录成功
			callback(user, errm);
		}, function(err) {
			//登录失败
			callback(sucm, err);
		});
	};

	//注册或者找回密码时候获取验证码
	m.getSmsCode = function(phone, callback) {
		// alert(phone + "---");
		AV.Cloud.requestSmsCode(phone).then(function() {
			//发送成功
			callback("0");
		}, function(err) {
			//发送失败
			callback(err);
		});
	};

	//验证获取的验证码
	m.signUpOrlogInByPhone = function(phone, identify, callback) {
		var user = new AV.User();
		var sucm;
		var errm;
		user.signUpOrlogInWithMobilePhone({
			mobilePhoneNumber : phone,
			smsCode : identify,
		}, {
			success : function(user) {
				//注册或者登录成功
				callback(user, errm);
			},
			error : function(err) {
				//失败
				callback(sucm, err);
			}
		});
	};

	//设置初始密码
	m.setPassword = function(phone, password, callback) {
		var sucm;
		var errm;
		var user = AV.User.current();
		user.setPassword(password);
		user.save().then(function(user) {
			//验证成功
			$mxuser.regToEasyCloud(phone, password, function(ret, errr) {
				if (ret) {
					callback(ret, errm);
				} else {
					callback(sucm, errr);
				}
			});
		}, function(err) {
			//验证失败
			callback(sucm, err);
		});
	};

	//重置密码
	m.resetPassword = function(password, callback) {
		var sucm;
		var errm;
		var user = AV.User.current();
		user.setPassword(password);
		user.save().then(function(user) {
			//验证成功
			callback(user, errm);
		}, function(err) {
			//验证失败
			callback(sucm, err);
		});
	};

	//去EasyCloud注册
	m.regToEasyCloud = function(phone, password, callback) {
		var sucm;
		var errm;
		AV.Cloud.run('registerToEasyCloud', {
			login_id : phone,
			password : password
		}, {
			success : function(data) {
				//调用成功，得到成功的应答data
				//在EasyCloud注册成功后还需要将token写到LeanCloud的用户列表里
				var usertoken = data.data.token;
				var user = AV.User.current();
				user.set('userToken', usertoken);
				user.save().then(function(user) {
					//验证成功
					callback(data, errm);
				}, function(errsave) {
					//验证失败
					callback(sucm, errsave);
				});
			},
			error : function(err) {
				//处理调用失败
				callback(sucm, err);
			}
		});
	};

	//修改当前用户密码
	m.updatePassword = function(oldpsw, password, callback) {
		var sucm;
		var errm;
		var user = AV.User.current();
		user.updatePassword(oldpsw, password, {
			success : function(ret) {
				//更新成功
				callback(ret, errm);
			},
			error : function(err) {
				//更新失败
				callback(sucm, err);
			}
		});
	};

	//修改当前用户昵称
	m.updateNickName = function(nickname, callback) {
		var sucm;
		var errm;
		var user = AV.User.current();
		user.set('nickname', nickname);
		user.save().then(function(user) {
			//验证成功
			callback(user, errm);
		}, function(err) {
			//验证失败
			callback(sucm, err);
		});
	};

	//修改当前用户邮箱
	m.updateEmail = function(email, callback) {
		var sucm;
		var errm;
		var user = AV.User.current();
		user.set('email', email);
		user.save().then(function(user) {
			//验证成功
			callback(user, errm);
		}, function(err) {
			//验证失败
			callback(sucm, err);
		});
	};

	//发送反馈信息
	m.sendFeedback = function(phone, content, callback) {
		var sucm;
		var errm;
		//		alert(APP_NAME+" "+phone+" "+content);
		//AV.Object
		var fd = AV.Object.new('feedback');
		fd.set("appname", APP_NAME);
		fd.set("phone", phone);
		fd.set("fdcontent", content);
		fd.save(null, {
			success : function(ret) {
				// Execute any logic that should take place after the object is saved.
				//				alert('New object created with objectId: ' + fd.id);
				//fd成功
				callback(ret, errm);
			},
			error : function(ret, err) {
				// Execute any logic that should take place if the save fails.
				// error is a AV.Error with an error code and description.
				//				alert('Failed to create new object, with error code: ' + err.message);
				//fd失败
				callback(sucm, err);
			}
		});
	};

	//判断用户是否存在
	m.isExist = function(username, callback) {
		var sucm;
		var errm;
		var query = new AV.Query(AV.User);
		query.equalTo("username", username);
		query.find({
			success : function(ret) {
				callback(ret, errm);
			},
			error : function(err) {
				callback(sucm, err);
			}
		});
	};

	//云代码测试
	m.cloudTest = function(phone, password, callback) {
		var sucm;
		var errm;
		AV.Cloud.run('loginEasyCloud', {
			login_id : phone,
			password : password
		}, {
			success : function(data) {
				callback(data, errm);
			},
			error : function(err) {
				//处理调用失败
				callback(sucm, err);
			}
		});
	};
	/*end*/

	window.$mxuser = m;

})(window);
