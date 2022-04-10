
# RobusTest Smart Switch 

This ll allow to controll a USB type connection to on or off via API. 

This will take a power from USB type C cabel from one end (`side - input side has 2 usb connection one for ESP board that is micro and other one is power.` )

Once its connected please follow screen direction, it ll ask to join a wifi network and once you join you have to configure it. 

It ll ask for wifi network(this network should have access to nerve/load balancer), 
1. wifi password 
2. nerve/balancer url
3. port
4. neuron key

once its connected, it ll add this device as node to nevre


**How to controll mobile charging**

make GET call 
Current one support 3 switch
- To off - `node_ip (please see the screen) :8080/relay/off?switch=SwitchNumber_This_case_it_ll_be_1_or_2_or_3`
- To On - `node_ip (please see the screen) :8080/relay/on?switch=SwitchNumber_This_case_it_ll_be_1_or_2_or_3`
- To get status - `node_ip (please see the screen) :8080/status`


**How to reset -** Make GET call to ` node_ip (please see the screen) :8080/reset` and wait for 10 second then diconnect the power and reconnect 

