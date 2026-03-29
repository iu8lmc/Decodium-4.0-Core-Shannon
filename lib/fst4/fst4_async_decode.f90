module fst4_async_mod
! Module for in-process FST4/FST4W decoding with result collection.

  use fst4_decode
  implicit none

  integer, parameter :: MAX_ASYNC_LINES = 200

  type, extends(fst4_decoder) :: async_fst4_dec
     integer :: nasync_lines = 0
     integer :: async_snrs(MAX_ASYNC_LINES)
     real :: async_dts(MAX_ASYNC_LINES)
     real :: async_freqs(MAX_ASYNC_LINES)
     integer :: async_naps(MAX_ASYNC_LINES)
     real :: async_quals(MAX_ASYNC_LINES)
     integer*1 :: async_bits(77,MAX_ASYNC_LINES)
     character(len=37) :: async_decoded(MAX_ASYNC_LINES)
     real :: async_fmids(MAX_ASYNC_LINES)
     real :: async_w50s(MAX_ASYNC_LINES)
  end type

contains

  subroutine async_fst4_callback(this, nutc, sync, snr, dt, freq, decoded, nap, qual, ntrperiod, fmid, w50, bits77)
    class(fst4_decoder), intent(inout) :: this
    integer, intent(in) :: nutc
    real, intent(in) :: sync
    integer, intent(in) :: snr
    real, intent(in) :: dt
    real, intent(in) :: freq
    character(len=37), intent(in) :: decoded
    integer, intent(in) :: nap
    real, intent(in) :: qual
    integer, intent(in) :: ntrperiod
    real, intent(in) :: fmid
    real, intent(in) :: w50
    integer*1, intent(in) :: bits77(77)

    select type(this)
    type is (async_fst4_dec)
       if(this%nasync_lines < MAX_ASYNC_LINES) then
          this%nasync_lines = this%nasync_lines + 1
          this%async_snrs(this%nasync_lines) = snr
          this%async_dts(this%nasync_lines) = dt
          this%async_freqs(this%nasync_lines) = freq
          this%async_naps(this%nasync_lines) = nap
          this%async_quals(this%nasync_lines) = qual
          this%async_bits(:,this%nasync_lines) = bits77
          this%async_decoded(this%nasync_lines) = decoded
          this%async_fmids(this%nasync_lines) = fmid
          this%async_w50s(this%nasync_lines) = w50
       endif
    end select
  end subroutine async_fst4_callback

end module fst4_async_mod


subroutine fst4_async_decode(iwave, nutc, nqsoprogress, nfa, nfb, nfqso, &
     ndepth, ntrperiod, nexp_decode, ntol, emedelay, nagain, lapcqonly, &
     mycall, hiscall, iwspr, lprinthash22, snrs, dts, freqs, naps, quals, &
     bits77, decodeds, fmids, w50s, nout)

! Standalone FST4/FST4W decoder for an in-process worker path.

  use fst4_async_mod

  integer*2 iwave(*)
  integer nutc, nqsoprogress, nfa, nfb, nfqso, ndepth, ntrperiod
  integer nexp_decode, ntol, nagain, lapcqonly, iwspr, lprinthash22
  real emedelay
  character*12 mycall, hiscall
  integer snrs(MAX_ASYNC_LINES)
  real dts(MAX_ASYNC_LINES)
  real freqs(MAX_ASYNC_LINES)
  integer naps(MAX_ASYNC_LINES)
  real quals(MAX_ASYNC_LINES)
  integer*1 bits77(77,MAX_ASYNC_LINES)
  character*37 decodeds(MAX_ASYNC_LINES)
  real fmids(MAX_ASYNC_LINES)
  real w50s(MAX_ASYNC_LINES)
  integer nout

  type(async_fst4_dec) :: dec
  integer i
  logical lagain
  logical lapcq
  logical lhash22

  dec%nasync_lines = 0
  dec%async_snrs = 0
  dec%async_dts = 0.0
  dec%async_freqs = 0.0
  dec%async_naps = 0
  dec%async_quals = 0.0
  dec%async_bits = 0
  dec%async_decoded = ' '
  dec%async_fmids = -999.0
  dec%async_w50s = 0.0
  snrs = 0
  dts = 0.0
  freqs = 0.0
  naps = 0
  quals = 0.0
  bits77 = 0
  decodeds = ' '
  fmids = -999.0
  w50s = 0.0
  lagain = nagain .ne. 0
  lapcq = lapcqonly .ne. 0
  lhash22 = lprinthash22 .ne. 0

  call dec%decode(async_fst4_callback, iwave, nutc, nqsoprogress, nfa, nfb, &
       nfqso, ndepth, ntrperiod, nexp_decode, ntol, emedelay, lagain, lapcq, &
       mycall, hiscall, iwspr, lhash22)

  nout = dec%nasync_lines
  do i = 1, nout
     snrs(i) = dec%async_snrs(i)
     dts(i) = dec%async_dts(i)
     freqs(i) = dec%async_freqs(i)
     naps(i) = dec%async_naps(i)
     quals(i) = dec%async_quals(i)
     bits77(:,i) = dec%async_bits(:,i)
     decodeds(i) = dec%async_decoded(i)
     fmids(i) = dec%async_fmids(i)
     w50s(i) = dec%async_w50s(i)
  enddo

  return
end subroutine fst4_async_decode
