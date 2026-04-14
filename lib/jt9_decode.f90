module jt9_decode
  integer, parameter :: JT9_NSMAX=6827

  type :: jt9_decoder
     procedure(jt9_decode_callback), pointer :: callback
     real*4 :: ccfred(JT9_NSMAX)=0.0
     real*4 :: red2(JT9_NSMAX)=0.0
   contains
     procedure :: decode
  end type jt9_decoder

  abstract interface
     subroutine jt9_decode_callback (this, sync, snr, dt, freq, drift, &
          decoded)
       import jt9_decoder
       implicit none
       class(jt9_decoder), intent(inout) :: this
       real, intent(in) :: sync
       integer, intent(in) :: snr
       real, intent(in) :: dt
       real, intent(in) :: freq
       integer, intent(in) :: drift
       character(len=22), intent(in) :: decoded
     end subroutine jt9_decode_callback
  end interface

contains

  subroutine decode(this,callback,ss,id2,nfqso,newdat,npts8,nfa,    &
       nfsplit,nfb,ntol,nzhsym,nagain,ndepth,nmode,nsubmode,nexp_decode)
    use timer_module, only: timer

    include 'constants.f90'
    class(jt9_decoder), intent(inout) :: this
    procedure(jt9_decode_callback) :: callback
    real ss(184,NSMAX)
    logical, intent(in) :: newdat, nagain
    character*22 msg
    logical ccfok(NSMAX)
    logical done(NSMAX)
    integer*2 id2(NTMAX*12000)
    integer*1 i1SoftSymbols(207)
    common/decstats/ntry65a,ntry65b,n65a,n65b,num9,numfano

    if(nexp_decode.eq.-99) stop     !Silence compiler warning
    this%callback => callback
    if(nmode.eq.9 .and. nsubmode.ge.1) then
       call decode9w(nfqso,ntol,nsubmode,ss,id2,sync,nsnr,xdt,freq,msg)
       if (associated(this%callback)) then
          ndrift=0
          call this%callback(sync,nsnr,xdt,freq,ndrift,msg)
       end if
       go to 999
    endif

    nsynced=0
    ndecoded=0
    nsps=6912                                   !Params for JT9-1
    df3=1500.0/2048.0

    tstep=0.5*nsps/12000.0                      !Half-symbol step (seconds)
    done=.false.

    nf0=0
    nf1=nfa
    if(nmode.eq.65+9) nf1=nfsplit
    ia=max(1,nint((nf1-nf0)/df3))
    ib=min(NSMAX,nint((nfb-nf0)/df3))
    lag1=-int(2.5/tstep + 0.9999)
    lag2=int(5.0/tstep + 0.9999)
    if(newdat) then
       call timer('sync9   ',0)
       call sync9(ss,nzhsym,lag1,lag2,ia,ib,this%ccfred,this%red2,ipk)
       call timer('sync9   ',1)
    endif

    nsps8=nsps/8
    df8=1500.0/nsps8
    dblim=db(864.0/nsps8) - 26.2

    ia1=1                         !quel compiler gripe
    ib1=1                         !quel compiler gripe
    do nqd=1,0,-1
       limit=5000
       ccflim=3.0
       red2lim=1.6
       schklim=2.2
       if(iand(ndepth,7).eq.2) then
          limit=10000
          ccflim=2.7
       endif
       if(iand(ndepth,7).eq.3 .or. nqd.eq.1) then
          limit=30000
          ccflim=2.5
          schklim=2.0
       endif
       if(nagain) then
          limit=100000
          ccflim=2.4
          schklim=1.8
       endif
       ccfok=.false.

       if(nqd.eq.1) then
          nfa1=nfqso-ntol
          nfb1=nfqso+ntol
          ia=max(1,nint((nfa1-nf0)/df3))
          ib=min(NSMAX,nint((nfb1-nf0)/df3))
          ccfok(ia:ib)=(this%ccfred(ia:ib).gt.(ccflim-2.0)) .and.          &
               (this%red2(ia:ib).gt.(red2lim-1.0))
          ia1=ia
          ib1=ib
       else
          nfa1=nf1
          nfb1=nfb
          ia=max(1,nint((nfa1-nf0)/df3))
          ib=min(NSMAX,nint((nfb1-nf0)/df3))
          do i=ia,ib
             ccfok(i)=this%ccfred(i).gt.ccflim .and. this%red2(i).gt.red2lim
          enddo
          ccfok(ia1:ib1)=.false.
       endif

       fgood=0.
       do i=ia,ib
          if(done(i) .or. (.not.ccfok(i))) cycle
          f=(i-1)*df3
          if(nqd.eq.1 .or.                                                   &
               (this%ccfred(i).ge.ccflim .and. abs(f-fgood).gt.10.0*df8)) then

             call timer('softsym ',0)
             fpk=nf0 + df3*(i-1)
             call softsym(id2,npts8,nsps8,newdat,fpk,syncpk,snrdb,xdt,    &
                  freq,drift,a3,schk,i1SoftSymbols)
             call timer('softsym ',1)

             sync=(syncpk+1)/4.0
             if(nqd.eq.1 .and. ((sync.lt.0.5) .or. (schk.lt.1.0))) cycle
             if(nqd.ne.1 .and. ((sync.lt.1.0) .or. (schk.lt.1.5))) cycle

             call timer('jt9fano ',0)
             call jt9fano(i1SoftSymbols,limit,nlim,msg)
             call timer('jt9fano ',1)

             if(sync.lt.0.0 .or. snrdb.lt.dblim-2.0) sync=0.0
             nsync=int(sync)
             if(nsync.gt.10) nsync=10
             nsnr=nint(snrdb)
             ndrift=nint(drift/df3)
             num9=num9+1

             if(msg.ne.'                      ') then
                numfano=numfano+1
                if (associated(this%callback)) then
                   call this%callback(sync,nsnr,xdt,freq,ndrift,msg)
                end if
                iaa=max(1,i-1)
                ibb=min(NSMAX,i+22)
                fgood=f
                nsynced=1
                ndecoded=1
                ccfok(iaa:ibb)=.false.
                done(iaa:ibb)=.true.
             endif
          endif
       enddo
       if(nagain) exit
    enddo

999 return
  end subroutine decode
end module jt9_decode
