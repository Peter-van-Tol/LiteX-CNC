=======
StepGen
=======

The module ``StepGen`` is used to control stepper motors. The maximum step rate is not limited by
software or CPU, but rather by the speed of the FPGA. Based on a 50 MHz FPGA the maximum step frequency
is tuned to be approximately 400 kHz. The maximum step frequency scales linearly with the FPGA frequency

In contrast to the `LinuxCNC stepgen component <https://linuxcnc.org/docs/html/man/man9/stepgen.9.html>`_, 
which has both *position*  and *velocity* modes, the module ``StepGen`` only has velocity mode. Velocity 
control drives the motor at a commanded speed, subject to accel and velocity limits. To convert the
position command to the required velocity command the component ``pos2vel`` can be used, which is part
of LitexCNC as well.

.. note::
    At this moment the timings can be set for each stepgen channel. At start up these timings are 
    aggregated to a single timing which is applied to the whole stepgen. This means that the slowest 
    drive will determine the maximum speed of the machine. In future release of LitexCNC this behavior
    will be changed and timings will be applied independently.

Step types
==========

The firmware of the ``stepgen`` consist of a step generator and a pin out. The pin out is depending on
the kind of step type which is driven by the channel, which may include:

step/dir
    Two pins, one for step and one for direction.
up/down
    Two pins, one for ’step up’ and one for ’step down’.
quadrature
    Two pins, phase-A and phase-B. For forward motion, A leads B.

Step types ``step/dir`` and ``up/down`` can be driven differential. This doubles the amount of physical
pins used (i.e. ``step-`` en ``step+``), but allows for faster driving of the drivers.

Configuration
=============

The code-block belows gives an example for the configuration of ``StepGen`` for different step types

.. tabs::

    .. code-tab:: json
        :caption: step/dir
        
        "stepgen": [
            {
                "pins" : {
                    "stepgen_type": "step_dir",
                    "step_pin": "j9:0",
                    "dir_pin": "j9:1"
                },
                "soft_stop": true
            },
            ...
        ]

    .. code-tab:: json
        :caption: step/dir (diff.)
        
        "stepgen": [
            {
                "pins" : {
                    "stepgen_type": "step_dir_differential",
                    "step_pos_pin": "j9:0",
                    "step_neg_pin": "j9:1",
                    "dir_pos_pin": "j9:2",
                    "dir_neg_pin": "j9:4"
                },
                "soft_stop": true
            },
            ...
        ]

HAL
===

.. note::
    The input and output pins are seen from the module. I.e. the GPIO In module will take an
    value from the machine and will put this on its respective _output_ pins. While the GPIO
    Out module will read the value from it input pins and put the value on the physical pins.
    This might feel counter intuitive at first glance.
    
Input pins
----------

<board-name>.stepgen.<n>.enable / <board-name>.stepgen.<name>.enable (HAL_BIT)
    Enables output steps - when false, no steps are generated and is the hardware disabled.
<board-name>.stepgen.<n>.velocity-cmd1 / <board-name>.stepgen.<name>.velocity-cmd1 (HAL_FLOAT)
    Commanded velocity for the first phase, in length units per second (see parameter
    position-scale).
<board-name>.stepgen.<n>.velocity-cmd2 / <board-name>.stepgen.<name>.velocity-cmd2 (HAL_FLOAT)
    Commanded velocity for the second phase, in length units per second (see parameter
    position-scale). When using the component ``pos2vel`` is used to convert the position
    command to velocity command, this pin should be set to the same value as ``velocity-cmd1``
<board-name>.stepgen.<n>.acceleration-cmd1 / <board-name>.stepgen.<name>.acceleration-cmd1 (HAL_FLOAT)
    The acceleration used to accelarate from the current velocity to ``velocity-cmd1``.
<board-name>.stepgen.<n>.acceleration-cmd2 / <board-name>.stepgen.<name>.acceleration-cmd2 (HAL_FLOAT)
    The acceleration used to accelarate from ``velocity-cmd1`` to ``velocity-cmd2``.

Output pins
-----------

<board-name>.stepgen.<n>.counts / <board-name>.stepgen.<name>.counts (HAL_UINT)
    The current position, in counts.
<board-name>.stepgen.<n>.position_fb / <board-name>.stepgen.<name>.position_fb (HAL_FLOAT)
    The received position from the FPGA in units.
<board-name>.stepgen.<n>.position_prediction / <board-name>.stepgen.<name>.position_prediction (HAL_FLOAT)
    The predicted position at the start of the next cycle. It is calculated based on the 
    ``position_fb``, and the commanded speeds and acceleration. This HAL-pin should be
     used asfeedback for ``motmod`` to prevent oscillations.
<board-name>.stepgen.<n>.speed_fb / <board-name>.stepgen.<name>.speed_fb (HAL_FLOAT)
    The current speed, in units per second.
<board-name>.stepgen.<n>.speed_prediction / <board-name>.stepgen.<name>.speed_prediction (HAL_FLOAT)
    The predicted speed at the start of the next cycle. It is calculated based on the 
    ``speed_fb``, and the commanded speeds and acceleration.

Parameters
----------

<board-name>.stepgen.<n>.frequency / <board-name>.stepgen.<name>.frequency (FLOAT / RO)
    The current step rate, in steps per second, for channel N.
<board-name>.stepgen.<n>.max-acceleration / <board-name>.stepgen.<name>.max-acceleration (FLOAT / RO)
    The acceleration/deceleration limit, in length units per second squared.
<board-name>.stepgen.<n>.max-velocity / <board-name>.stepgen.<name>.max-velocity (FLOAT / RO)
    The maximum allowable velocity, in length units per second. 
<board-name>.stepgen.<n>.position-scale / <board-name>.stepgen.<name>.position-scale (FLOAT / RO)
    The scaling for position feedback, position command, and velocity command, in steps per length unit.

There are five timing parameters which control the output waveform.  No step type uses all five, and
only those which will be used are exported to HAL.  The values of these parameters are in nano-seconds,
In the timing diagrams that follow, they are identfied by the following numbers:

1. 'steplen' = length of the step pulse.
2. 'stepspace' = minimum space between step pulses, space is dependent on the commanded speed. The check
   whether the minimum step space is obeyed is done in the driver.
3. 'dirhold_time' = minimum delay after a step pulse before a direction - may be longer
4. 'dir_setup_time' = minimum delay after a direction change and before the next step - may be longer

Timing parameters - step/dir
^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The timing diagram for both ``step/dir`` is shown below. There is no Difference
in timing diagram when differential output is used.

.. code-block::   

               _____         _____               _____
    STEP  ____/     \_______/     \_____________/     \______
              |     |       |     |             |     |
    Time      |-(1)-|--(2)--|-(1)-|--(3)--|-(4)-|-(1)-|
                                          |__________________
    DIR   ________________________________/

The relevant parameters which are exported to the HAL are:

<board-name>.stepgen.<n>.steplen / <board-name>.stepgen.<name>.steplen (FLOAT)
    The length of the step pulses, in nanoseconds. Measured from rising edge to falling edge.
<board-name>.stepgen.<n>.stepspace / <board-name>.stepgen.<name>.stepspace (FLOAT)
    Space between step pulses, in nanoseconds. Measured from falling edge to rising edge. The 
    actual time depends on the step rate and can be much longer. 
<board-name>.stepgen.<n>.dir-hold-time / <board-name>.stepgen.<name>.dir-hold-time (FLOAT)
    The minimum hold time of direction after step, in nanoseconds. Measured from falling 
    edge of step to change of direction.
<board-name>.stepgen.<n>.dir-setup-time / <board-name>.stepgen.<name>.dir-setup-time (FLOAT)
    The minimum setup time from direction to step, in nanoseconds periods. Measured from 
    change of direction to rising edge of step.

Timing parameters - up/down
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Not implemented yet.

Timing parameters - quadrature
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Not implemented yet.

Example
-------

The code below gives an example for a single axis, using the ``step-dir`` step type.

.. code-block::

    loadrt [KINS]KINEMATICS
    loadrt [EMCMOT]EMCMOT servo_period_nsec=[EMCMOT]SERVO_PERIOD num_joints=[KINS]JOINTS
    loadrt litexcnc
    loadrt litexcnc_eth config_file="[LITEXCNC]CONFIG_FILE"
    loadrt pos2vel number=1

    # Add the functions to the thread
    addf [LITEXCNC](NAME).read servo-thread
    addf motion-command-handler servo-thread
    addf motion-controller servo-thread
    addf pos2vel.convert servo-thread
    addf [LITEXCNC](NAME).write servo-thread

    [...]

    ########################################################################
    STEPGEN
    ########################################################################
    # - timings (prevent re-calculation)
    net pos2vel.period-s       <= [LITEXCNC](NAME).stepgen.period-s
    net pos2vel.period-s-recip <= [LITEXCNC](NAME).stepgen.period-s-recip

    STEPGEN - X-AXIS
    ########################################################################
    # POS2VEL
    # - position control
    net xpos-fb  <= [LITEXCNC](NAME).stepgen.00.position_prediction
    net xpos-fb  => joint.0.motor-pos-fb
    net xpos-fb  => pos2vel.0.position-feedback
    net xvel-fb  pos2vel.0.velocity-feedback <= [LITEXCNC](NAME).stepgen.00.velocity-prediction
    net xpos-cmd pos2vel.0.position-cmd      <= joint.0.motor-pos-cmd
    # - settings
    setp pos2vel.0.max-acceleration [JOINT_2]STEPGEN_MAXACCEL
    # setp pos2vel.0.debug 1

    # STEPGEN
    # - Setup of timings
    setp [LITEXCNC](NAME).stepgen.00.position-scale   [JOINT_2]SCALE
    setp [LITEXCNC](NAME).stepgen.00.steplen          5000
    setp [LITEXCNC](NAME).stepgen.00.stepspace        5000
    setp [LITEXCNC](NAME).stepgen.00.dir-hold-time    10000
    setp [LITEXCNC](NAME).stepgen.00.dir-setup-time   10000
    setp [LITEXCNC](NAME).stepgen.00.max-velocity     [JOINT_2]MAX_VELOCITY
    setp [LITEXCNC](NAME).stepgen.00.max-acceleration [JOINT_2]STEPGEN_MAXACCEL
    # setp [LITEXCNC](NAME).stepgen.00.debug 1
    # - Connect velocity command
    net xvel-cmd <= pos2vel.0.velocity-cmd
    net xvel-cmd => [LITEXCNC](NAME).stepgen.00.velocity-cmd1
    net xvel-cmd => [LITEXCNC](NAME).stepgen.00.velocity-cmd2
    # - Set the acceleration to be used (NOTE: pos2vel has fixed acceleration)
    setp [LITEXCNC](NAME).stepgen.00.acceleration-cmd1 [JOINT_2]STEPGEN_MAXACCEL
    setp [LITEXCNC](NAME).stepgen.00.acceleration-cmd2 [JOINT_2]STEPGEN_MAXACCEL
    # - enable the drive
    net xenable joint.0.amp-enable-out => [LITEXCNC](NAME).stepgen.00.enable


Break-out boards
================

...
