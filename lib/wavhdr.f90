module wavhdr
  use wsjtx_wav_catalog, only: NBANDS, NMODES, NPERIODS, WSJTX_BANDS, &
       WSJTX_MODES, WSJTX_PERIODS
  type hdr
     character*4 ariff
     integer*4 lenfile
     character*4 awave
     character*4 afmt
     integer*4 lenfmt
     integer*2 nfmt2
     integer*2 nchan2
     integer*4 nsamrate
     integer*4 nbytesec
     integer*2 nbytesam2
     integer*2 nbitsam2
     character*4 adata
     integer*4 ndata
  end type hdr

  contains

    function default_header(nsamrate,npts)
      type(hdr) default_header,h
      h%ariff='RIFF'
      h%awave='WAVE'
      h%afmt='fmt '
      h%lenfmt=16
      h%nfmt2=1
      h%nchan2=1
      h%nsamrate=nsamrate
      h%nbitsam2=16
      h%nbytesam2=h%nbitsam2 * h%nchan2 / 8
      h%adata='data'
      h%nbytesec=h%nsamrate * h%nbitsam2 * h%nchan2 / 8
      h%ndata=2*npts
      h%lenfile=h%ndata + 44 - 8
      default_header=h
    end function default_header

    subroutine set_wsjtx_wav_params(fMHz,mode,nsubmode,ntrperiod,id2)

      character*8 mode
      integer*2 id2(4)
      real fband(NBANDS)
      data fband/0.137,0.474,1.8,3.5,5.1,7.0,10.14,14.0,18.1,21.0,24.9,  &
           28.0,50.0,144.0,222.0,432.0,902.0,1296.0,2304.0,3400.0,       &
           5760.0,10368.0,24048.0/

      dmin=1.e30
      iband=0
      do i=1,NBANDS
         if(abs(fMHz-fband(i)).lt.dmin) then
            dmin=abs(fMHz-fband(i))
            iband=i
         endif
      enddo

      imode=0
      do i=1,NMODES
         if(mode.eq.WSJTX_MODES(i)) imode=i
      enddo

      ip=0
      do i=1,NPERIODS
         if(ntrperiod.eq.WSJTX_PERIODS(i)) ip=i
      enddo

      id2(1)=iband
      id2(2)=imode
      id2(3)=nsubmode
      id2(4)=ip
      
      return
    end subroutine set_wsjtx_wav_params

    subroutine get_wsjtx_wav_params(id2,band,mode,nsubmode,ntrperiod,ok)

      character*8 mode
      character*6 band
      integer*2 id2(4)
      logical ok

      ok=.true.
      if(id2(1).lt.1 .or. id2(1).gt.NBANDS) ok=.false.
      if(id2(2).lt.1 .or. id2(2).gt.NMODES) ok=.false.
      if(id2(3).lt.1 .or. id2(3).gt.8) ok=.false.
      if(id2(4).lt.1 .or. id2(4).gt.NPERIODS) ok=.false.

      if(ok) then
         band=WSJTX_BANDS(id2(1))
         mode=WSJTX_MODES(id2(2))
         nsubmode=id2(3)
         ntrperiod=WSJTX_PERIODS(id2(4))
      endif

      return
    end subroutine get_wsjtx_wav_params

end module wavhdr
