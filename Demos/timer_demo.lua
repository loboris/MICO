--timer demo
print("------timer demo------")
function tmr_cb()
	print('tmr: '..testtmr..', tick: '..tmr.tick()) 
end

testtmr = tmr.find()
tmr.start(testtmr, 3000, tmr_cb)

print("Input command: tmr.stop(testtmr) to stop timer")
--stop timer
--tmr.stop(testtmr)

--stop all timer
--tmr.stopall()

--delay in ms
--tmr.delayms(1000)

--restore watchdog counter
--tmr.wdclr()