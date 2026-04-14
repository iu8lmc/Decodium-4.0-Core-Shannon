module fast_decode_contexts
  implicit none

  integer, parameter :: FAST_DECODE_NMAX=30*12000
  integer, parameter :: FAST_DECODE_NFFT=262145

  type fast_decode_state
     logical :: initialized=.false.
     integer :: npts=0
     integer :: nutca=0
     integer :: nutcb=0
     integer*2 :: id2a(FAST_DECODE_NMAX)
     integer*2 :: id2b(FAST_DECODE_NMAX)
     complex :: cdat(FAST_DECODE_NFFT)
     complex :: cdat2(FAST_DECODE_NFFT)
  end type fast_decode_state

  type(fast_decode_state), save, target :: jt9_fast_state
  type(fast_decode_state), save, target :: msk144_fast_state

contains

  subroutine init_fast_decode_state(state)
    type(fast_decode_state), intent(inout) :: state

    if(state%initialized) return

    state%id2a=0
    state%id2b=0
    state%cdat=(0.0,0.0)
    state%cdat2=(0.0,0.0)
    state%npts=0
    state%nutca=0
    state%nutcb=0
    state%initialized=.true.
  end subroutine init_fast_decode_state

end module fast_decode_contexts

subroutine fast_decode(id2,narg,trperiod,line,mycall_12,   &
     hiscall_12)

  use fast_decode_contexts, only: fast_decode_state, jt9_fast_state,     &
       msk144_fast_state, init_fast_decode_state, FAST_DECODE_NMAX

  integer*2 id2(FAST_DECODE_NMAX)
  integer narg(0:14)
  double precision trperiod
  real, allocatable :: dat(:)
  logical pick
  character*6 cfile6
  character*80 line(100)
  character*12 mycall_12,hiscall_12
  character*6 mycall,hiscall
  type(fast_decode_state), pointer :: state

  mycall=mycall_12(1:6)
  hiscall=hiscall_12(1:6)
  nutc=narg(0)
  ndat0=narg(1)
  nsubmode=narg(2)
  newdat=narg(3)
  minsync=narg(4)
  npick=narg(5)
  t0=0.001*narg(6)
  t1=0.001*narg(7)
  tmid=0.5*(t0+t1)
  maxlines=narg(8)
  nmode=narg(9)
  nrxfreq=narg(10)
  ntol=narg(11)
  nhashcalls=narg(12)

  if(nmode.eq.102) then
     state => jt9_fast_state
  else
     state => msk144_fast_state
  endif
  call init_fast_decode_state(state)

  line(1:100)(1:1)=char(0)
  if(t0.gt.trperiod) go to 900
  if(t0.gt.t1) go to 900

  if(nmode.eq.102) then
     call fast9(id2,narg,line)
     go to 900
  endif

  allocate(dat(30*12000))

  if(newdat.eq.1) then
     state%cdat2=state%cdat
     ndat=ndat0
     call wav11(id2,ndat,dat)
     nzz=11025*int(trperiod)    !beware if fractional T/R period ever used here
     if(ndat.lt.nzz) dat(ndat+1:nzz)=0.0
     ndat=min(ndat,30*11025)
     call ana932(dat,ndat,state%cdat,state%npts) !Make downsampled analytic signal
  endif

! Now cdat() is the downsampled analytic signal.
! New sample rate = fsample = BW = 11025 * (9/32) = 3100.78125 Hz
! NB: npts, nsps, etc., are all reduced by 9/32

  write(cfile6,'(i6.6)') nutc
  nfreeze=1
  mousedf=0
  mousebutton=0
  mode4=1
  if(nsubmode.eq.1) mode4=2
  nafc=0
  ndebug=0
  t2=0.
  ia=1
  ib=state%npts
  pick=.false.

  if(npick.gt.0) then
     pick=.true.
     dt=1.0/11025.0 * (32.0/9.0)
     ia=t0/dt + 1.
     ib=t1/dt + 1.
     t2=t0
  endif
  jz=ib-ia+1
  line(1:100)(1:1)=char(0)

900 if(allocated(dat)) deallocate(dat)
  return
end subroutine fast_decode
