# CSE-474
Smart Pet Monitoring System

**Authors:** Julia Yu, Giannah Donahoe

Date: 3/19/2026

**Introduction**

For the final project, we designed a "Smart Pet Monitoring System" designed to automate the process of monitoring a house pet, ensuring the owner knows the condition of their pet’s environment at all times. By integrating sensors with an ESP32, the system will monitor the surrounding environment’s temperature, humidity, sound levels, and motion while also checking the water level of the pet’s drinking bowl. Actuators are introduced to lower ambient temperature with a fan and motorized window mechanism. Throughout this project we aim to ensure reliable timing and I/O operations, operate under high speed and CPU load, interface with novel hardware, manipulate physical parameters, develop a user interface, utilize dual core operations, and implement a queue for data handling. Our learning objectives include applying embedded system design principles, optimizing system performance, and problem-solving and innovation.


**System Design and Implementation**

Our system uses FreeRTOS to manage and schedule tasks. In total, it includes six tasks: the sensor task, message task, buzzer task, fan task, window task, and LCD task. The sensor task reads sensor values for water level, temperature, humidity, sound, and motion. The message task sends the average value of each sensor over a fixed time period to a Bluetooth device. The buzzer task activates the buzzer when the real temperature, calculated from the raw temperature and raw humidity, becomes too high. The fan task controls a stepper motor to turn on the fan when the temperature exceeds a threshold. The window task controls a servo motor to open the window when the humidity becomes too high. The LCD task displays each sensor reading on the LCD screen.

We use two periodic timers in the system. One timer fires every second to trigger sensor readings, and the other fires every 100 seconds to send the average sensor values to the Bluetooth device. Two global variables are used to store the current sensor values and the current average sensor values. A mutex protects these global variables from race conditions. A queue is used to pass sensor values to the LCD task so that each value can be displayed one at a time in a rotating sequence. We define a SensorData struct to represent all sensor readings in a single data object. In addition, we use a separate LCDValue struct, which includes both the sensor type and its corresponding value, to transmit data through the queue to the LCD task for display. Finally, we use two button interrupts, one for the fan and one for the window, to allow manual control of these two components. Each button press toggles the system between manual and automatic modes. When switched to manual mode, the component state (on/off) is also toggled. The next button press returns the system to automatic mode.

**Code Structure**

Our code is structured around the six tasks, two timer interrupts, and two button interrupts described in the Code Design section above. In the setup function, we initialize all system components, including BLE communication, the LCD, mutex, sensor pins, servo motor, stepper motor, buzzer, queue, and 2 button interrupts. We also create the six FreeRTOS tasks and configure the two ESP timers.

To better modularize, we created a helper function find_moving_average, which computes averaged sensor values for transmission in the message task. Overall, the code organization separates initialization, task logic, and utility functions, improving maintainability while avoiding redundancy.

**Discussion**

Video Demonstrating Project Working

Most of the challenges we encountered were related to motor control. For the stepper motor, we found that it only operated within certain speed and step configurations, so we used a trial-and-error approach to determine a stable maximum speed. Additionally, because the stepper motor is used to simulate a fan, large delays in the task caused uneven motion. To address this, we reduced the step size and introduced shorter delays, allowing smoother movement while still yielding time for other tasks to execute.

For the window task (servo motor), the motor does not require continuous updates, so we introduced a delay of one second between operations. This reduces unnecessary processing but introduces a trade-off: the system may take up to one second to respond to repeated button presses.

Another challenge involved switching between automatic and manual modes. We initially encountered bugs when handling button presses, particularly with inconsistent mode transitions. This was resolved by using global state variables updated within the button interrupts.


**Division of Work**

In terms of workload distribution, Giannah focused on implementing the sensor-related functionality, including the sensor task, sensor timer, LCD task, and buzzer task on Core 0. She also worked on Doxgen integration and implemented the physical structure of the project. This amounted to 30% of the work. Julia designed the data structures (SensorData and LCDValue) and implemented the queue and mutex for safe data sharing, and developed the fan and window tasks with their corresponding button interrupts. She also implemented the message timer and message task with BLE communication on Core 1. This constituted 40% of the project. Throughout the project, both members collaborated on testing, debugging, and refining system behavior to ensure all components worked together reliably as well as developing the final report. This portion of the project summed to 30% of the work.


**Suggestions**

For the final project, more clear instructions on how to use the new components such as how to connect the sensors and the motors to ESP32 would be helpful. There may be benefit in structured check-ins with the TAs for determining progress and receiving feedback throughout the design and implementation stages. This would allow for a more collaborative final project between the instruction staff and students.


**Conclusions**

This final project successfully implements a complex, real-time embedded system using FreeRTOS on the ESP32 platform. We explored advanced scheduling techniques, interfaced with novel hardware components, and developed user-friendly interfaces. Numerous skills were gained, but an emphasis was held on effective time management, high performance under CPU load, and the integration of dual-core operations to achieve parallel task execution. This project has been valuable, providing experience in the growing field of smart home devices and the Internet of Things (IoT). We demonstrated our ability to manage tasks, synchronization, and timing, culminating in a functional system that addresses a real-world problem.

**References**

[1]  Lady Ada. “Adafruit AM2320 Sensor.” Adafruit Industries. https://cdn-learn.adafruit.com/downloads/pdf/adafruit-am2320-temperature-humidity-i2c-sensor.pdf

[2] Jobit Joseph. “Arduino Interfacing with Water Level Sensor.” Digi-Key Maker.io, 2024. https://www.digikey.lt/en/maker/projects/arduino-interfacing-with-water-level-sensor/f6d1f7589f9e44b7a2b6993e4931c9f5

[3] Øyvind Nydal Dahl. “Arduino Sound Sensor: Control an LED with Sound.” buildelectroniccircuits, 2023. https://www.build-electronic-circuits.com/arduino-sound-sensor/

[4] “Interfacing Arduino Uno with PIR Motion Sensor.” Arduino Project Hub, 2021. https://eeshop.unl.edu/pdf/Stepper+Driver.pdf

[5] “Stepper Motor 5V 4-Phase 5-Wire & ULN2003 Driver Board for Arduino.” Geeetech Wiki, 2012. https://www.build-electronic-circuits.com/arduino-sound-sensor/
