
-- ** you can test the boot reason
--[[
if mcu.bootreason() ~= "PWRON_RST" and mcu.bootreason() ~= "BOR_RST" then
    return
end
]]--

-- *** you can set you wifi credentials, and start wifi **
--wifi.startsta({ssid="mySSID", pwd="myWiFiKey"})

-- ** Get the time from ntp server **
-- ** wifi.sta.ntptime(time_zone,"ntp_server", report)
--wifi.sta.ntptime(1)

-- ** OR you can use saved wifi credentials from system parameters
--    and just call "wifi.startsta" WITH empty parameter **
--    ntp server will be started automatically
wifi.startsta({})
