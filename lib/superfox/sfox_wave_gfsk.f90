subroutine sfox_wave_gfsk()

! Called by WSJT-X when it's time for SuperFox to transmit.  Reads array
! itone(1:151) from disk file 'sfox_2.dat' in the writable data directory.
! Generates a GFSK waveform with short ramp-up and ramp-down symbols of 
! duration NSPS/BT at the beginning and end of the waveform.

  parameter (NWAVE=(160+2)*134400*4) !Max WSJT-X waveform (FST4-1800 at 48kHz)
  parameter (NSYM=151,NSPS=1024*4)
  parameter (NPTS=(NSYM+2)*NSPS)
  parameter (BT=8)
  character*40 cmsg2
  integer itone(151)
  real*8 dt,twopi,f0,phi,dphi_peak
  real*8, allocatable :: dphi(:)
  real*8 pulse(3*NSPS)
  logical first/.true./

  common/foxcom/wave(NWAVE)
  common/foxcom3/nslots2,cmsg2(5),itone3(151)
  save first,twopi,dt,hmod,dphi_peak,pulse

  allocate(dphi(0:NPTS-1))

  if(first) then
    fsample=48000.0
    twopi=8.d0*atan(1.d0)
    dt=1.d0/fsample
    hmod=1.0
    dphi_peak=twopi*hmod/real(NSPS)
    do i=1,3*NSPS 
      tt=(i-1.5*NSPS)/real(NSPS)
      pulse(i)=gfsk_pulse(BT,tt)
    enddo
    first=.false.
  endif
  wave=0.
 
  itone=itone3
  if(itone(1).lt.0 .or. itone(1).gt.128) go to 999

! Generate the SuperFox waveform.
  
  dphi=0.d0
  do j=1,NSYM
    ib=(j-1)*NSPS
    ie=ib+3*NSPS-1
    dphi(ib:ie)=dphi(ib:ie)+dphi_peak*pulse(1:3*NSPS)*itone(j)
  enddo
  dphi(0:2*NSPS-1)=dphi(0:2*NSPS-1)+dphi_peak*itone(1)*pulse(NSPS+1:3*NSPS)
  dphi(NSYM*NSPS:(NSYM+2)*NSPS-1)=dphi(NSYM*NSPS:(NSYM+2)*NSPS-1)+dphi_peak*itone(NSYM)*pulse(1:2*NSPS)

  phi=0.d0
  f0=750.0d0
  dphi=dphi+twopi*f0*dt
  k=0
  do j=1,NSPS*(NSYM+2)-1
    k=k+1
    wave(k)=sin(phi)
    phi=phi+dphi(j)
  enddo
  
! Add raised cosine ramps at the beginning and end of the waveform.
! Since the modulator expects an integral number of symbols, dummy
! symbols are added to the beginning and end of the waveform to 
! hold the ramps. All but nramp of the samples in each dummy
! symbol will be zero.
 
  nramp=NSPS/BT
  wave(1:NSPS-nramp)=0.0
  wave(NSPS-nramp+1:NSPS)=wave(NSPS-nramp+1:NSPS) *                   &
       (1.0-cos(twopi*(/(i,i=0,nramp-1)/)/(2.0*nramp)))/2.0
  k1=(NSYM+1)*NSPS+1
  wave(k1:k1+nramp-1)=wave(k1:k1+nramp-1) *                           &
       (1.0+cos(twopi*(/(i,i=0,nramp-1)/)/(2.0*nramp)))/2.0
  wave(k1+nramp:NPTS)=0.0

999 return
end subroutine sfox_wave_gfsk
