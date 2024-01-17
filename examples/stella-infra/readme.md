# Demo / Evaluation 

- timekeeping across power-failures
- power consumption

	- computing
	- communication
	- sleep
	- power off

## Use-Case

- audio sampling with fixed T_dur, f_s
- audio processing e.g. FFT
- transmission to basestation

	- sample counter
	- pkt id
	- (timestamp) (???)

## Highlights

Capacitor

- too small: sampling fails (atomic OP)
- bigger: works
- much bigger: bursts, but largest intervals: instructive (???)

Energy Input

- to high: unnecessarily high sampling rate
- lower: perfect

Checkpointing

- built-in, no special care needed
- pkt-id and sample counter retained across power failure

- Q: does the checkpoint scale with program-size? how long does it take? (with uart enabled) i saw delays of 10 to 90 s with stella
	- `RAM_RETAINED_SIZE` and `STACK_SIZE` 
	-> how do I see currently used resources of just my 'app'?
	-> could this process be automatic?
- Q: what about new FW but old checkpoint? How does it differentiate?
