/**
 * Created by rocke on 15-5-1.
 */

$(function() {
	//登录
	$("#loginbtn").click(function() {
		var phone = $("#login_phone").val();
		var psw = $("#login_psw").val();
		if (true != isphone2(phone)) {
			apiToast("请确认手机号", 2000);
		} else {
			showProgress("登陆中", true);
			$mxuser.loginWithPhone(phone, psw, function(ret, err) {
				if (ret) {
					if (strDMlogin == 'ios') {
						openIndexWin();
						hidPro();
					} else {
						openDevlistWin();
						hidPro();
					}
				} else {
					hidPro();
					if (err.message.indexOf("Could not find user") >= 0) {
						apiToast("用户不存在", 2000);
					} else {
						apiToast("用户名或者密码不对", 2000);
					}
				}
			});
		}
	});

	//	发送验证码
	$("#getverifyCode").click(function() {
		if (0 == sendSmsTag) {
			var phone = $("#register_phone").val();
			var pswtag = $("#pswtag").val();
			if (true != isphone2(phone)) {
				apiToast(CONFIRM_PHONE, 2000);
			} else {
				//按钮变灰不可用，并开启倒计时
				$("#getverifyCode").removeClass("sendverify");
				$("#getverifyCode").addClass("sendverifyhs");
				sendSmsTag = 1;
				timecd = 60;
				$mxuser.isExist(phone, function(ret, err) {
					var ifreg_suc, length = ret.length;
					if (1 == length) {
						//看看userToken是否已经存在
						ifreg_suc = ret[0].get("userToken");
					}
					//如果是找回验证码pswtag=0：判断是否有此用户length，此用户是否有userToken
					//如果是注册pswtag=1：判断此用户是否已经注册length，此用户是否有userToken
					if (((0 == pswtag) && (0 < length) && ("undefined" != typeof (ifreg_suc))) || ((1 == pswtag) && (0 == length)) || ((1 == pswtag) && ("undefined" == typeof (ifreg_suc)))) {
						$mxuser.getSmsCode(phone, function(ret) {
							hidPro();
							//发送成功
							if (ret == 0) {
								apiToast(SEND_TO_PHONE, 2000);
							} else {
								alert(JSON.stringify(ret));
							}
						});
					} else {
						if (0 == pswtag) {
							apiToast(NOT_EXIST, 2000);
						} else if (1 == pswtag) {
							apiToast(IS_EXIST, 2000);
						}
						//恢复send按钮的初始状态
						window.clearInterval(ctrlSendSms);
						$("#getverifyCode").removeClass("sendverifyhs");
						$("#getverifyCode").addClass("sendverify");
						$("#sendsmstxt").text("发送");
						sendSmsTag = 0;
					}
				});
				ctrlSendSms = self.setInterval("setCountDown()", 1000);
			}
		} else {
			apiToast(SMS_FRE, 2000);
		}
	});
	//注册用户
	$("#nextstep").click(function() {
		var phone = $("#register_phone").val();
		var identify = $("#register_code").val();
		if (true != isphone2(phone)) {
			apiToast(CONFIRM_PHONE, 2000);
		} else if ("" == identify) {
			apiToast(VCODE_N_EMPTY, 2000);
		} else {
			showProgress(VALID_MSG, true);
			$mxuser.signUpOrlogInByPhone(phone, identify, function(ret, err) {
				if (ret) {
					apiToast(EDI_SUCCESS, 2000);
					$.mobile.changePage("#nextsteppage", {
						transition : "none"
					});
					hidPro();
				} else {
					apiToast(VALID_MSG_FAIL, 2000);
					hidPro();
				}
			});
		}
	});

	//退出登录
	$("#logout").click(function() {
		AV.User.logOut();
		currentUser = AV.User.current();
		window.location.href = "./login.html";
	});

	//设置初始密码
	$("#finishregister").click(function() {
		var phone = $("#register_phone").val();
		var password = $("#nextstep_psw").val();
		var confirmpsw = $("#nextstep_ckpsw").val();
		var pswtag = $("#pswtag").val();
		//1 初次设置密码 0重置密码
		if ("" == password) {
			apiToast(PSW_N_EMPTY, 2000);
		} else if (password != confirmpsw) {
			apiToast(PSW_N_MATCH, 2000);
		} else {
			showProgress(SETTING, true);
			if (pswtag == "1") {
				$mxuser.setPassword(phone, password, function(ret, err) {
					if (ret) {
						apiToast(EDI_SUCCESS, 2000);
						if (strDMlogin == 'ios') {
							openIndexWin();
							hidPro();
						} else {
							openDevlistWin();
							hidPro();
						}
					} else {
						hidPro();
						alert(JSON.stringify(err));
					}
				});
			} else {
				$mxuser.resetPassword(password, function(ret, err) {
					if (ret) {
						apiToast(EDI_SUCCESS, 2000);
						if (strDMlogin == 'ios') {
							openIndexWin();
							hidPro();
						} else {
							openDevlistWin();
							hidPro();
						}
					} else {
						hidPro();
						alert(JSON.stringify(err));
					}
				});
			}
		}
	});

});

/*判断输入是否为合法的手机号码*/
function isphone2(inputString) {
	var regBox = {
		regEmail : /^([a-z0-9_\.-]+)@([\da-z\.-]+)\.([a-z\.]{2,6})$/, //邮箱
		regName : /^[a-z0-9_-]{3,16}$/, //用户名
		regMobile : /^0?1[3|4|5|7|8][0-9]\d{8}$/, //手机
		regTel : /^0[\d]{2,3}-[\d]{7,8}$/
	};
	var mflag = regBox.regMobile.test(inputString);
	if (!mflag) {
		return false;
	} else {
		return true;
	};
}

function openIndexWin(){
	api.openWin({
		name : "index",
		url : "../index.html",
		reload : true,
		bgColor : '#F0F0F0',
		slidBackEnabled : false,
		animation : {
			type : 'none'
		}
	})
}
function openDevlistWin(){
	api.openWin({
		name : 'devlist',
		url : './devlist.html',
		reload : true,
		bgColor : '#F0F0F0',
		slidBackEnabled : false,
		animation : {
			type : 'none'
		}
	});
}

//发送短信的倒计时程序
function setCountDown() {
	if (timecd > 0) {
		$("#sendsmstxt").text(timecd-- + "s");
	} else {
		//恢复send按钮的初始状态
		window.clearInterval(ctrlSendSms);
		$("#getverifyCode").removeClass("sendverifyhs");
		$("#getverifyCode").addClass("sendverify");
		$("#sendsmstxt").text("发送");
		sendSmsTag = 0;
	}
}

//从底部的弹窗，毫秒级
function apiToast(msg, time) {
	api.toast({
		msg : msg,
		duration : time,
		location : 'bottom'
	});
}

/**
 *关闭APP
 */
function closeApp() {
	api.closeWidget({
		id : 'A6983536860165',
		retData : {
			name : 'closeWidget'
		},
		animation : {
			type : 'flip',
			subType : 'from_bottom',
			duration : 500
		}
	});
}

/**
 * 进度条
 */
//等待进度，（可选项）是否模态，模态时整个页面将不可交互
function showProgress(text, ifctrl) {
	api.showProgress({
		style : 'default',
		animationType : 'zoom',
		title : 'LOADING...',
		text : text,
		modal : ifctrl
	});
}

//隐藏浮动框
function hidPro() {
	api.hideProgress();
}

//获取用户登录的信息
function getUserInfo() {
	var userinfo = AV.User.current();
	return userinfo;
}