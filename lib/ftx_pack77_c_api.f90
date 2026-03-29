module ftx_pack77_c_api

  use, intrinsic :: iso_c_binding, only: c_bool, c_char, c_int, c_signed_char
  implicit none
  private

  integer, parameter, public :: MAXRECENT=10

  public :: ftx_pack77_reset_context
  public :: ftx_pack77_set_context
  public :: ftx_pack77_pack
  public :: ftx_pack77_unpack
  public :: ftx_pack77_save_hash_call
  public :: ftx_pack77_set_dxbase
  public :: ftx_pack77_pack28
  public :: ftx_pack77_unpack28
  public :: ftx_pack77_split77
  public :: ftx_pack77_packtext77
  public :: ftx_pack77_unpacktext77
  public :: ftx_pack77_hash_call

  ! Keep the legacy entry names available without maintaining duplicate wrappers.
  public :: legacy_pack77_reset_context
  public :: legacy_pack77_set_context
  public :: legacy_pack77_pack
  public :: legacy_pack77_unpack
  public :: legacy_pack77_save_hash_call
  public :: legacy_pack77_set_dxbase
  public :: legacy_pack77_pack28
  public :: legacy_pack77_unpack28
  public :: legacy_pack77_split77
  public :: legacy_pack77_packtext77
  public :: legacy_pack77_unpacktext77

  interface legacy_pack77_reset_context
     module procedure ftx_pack77_reset_context
  end interface legacy_pack77_reset_context

  interface legacy_pack77_set_context
     module procedure ftx_pack77_set_context
  end interface legacy_pack77_set_context

  interface legacy_pack77_pack
     module procedure ftx_pack77_pack
  end interface legacy_pack77_pack

  interface legacy_pack77_unpack
     module procedure ftx_pack77_unpack
  end interface legacy_pack77_unpack

  interface legacy_pack77_save_hash_call
     module procedure ftx_pack77_save_hash_call
  end interface legacy_pack77_save_hash_call

  interface legacy_pack77_set_dxbase
     module procedure ftx_pack77_set_dxbase
  end interface legacy_pack77_set_dxbase

  interface legacy_pack77_pack28
     module procedure ftx_pack77_pack28
  end interface legacy_pack77_pack28

  interface legacy_pack77_unpack28
     module procedure ftx_pack77_unpack28
  end interface legacy_pack77_unpack28

  interface legacy_pack77_split77
     module procedure ftx_pack77_split77
  end interface legacy_pack77_split77

  interface legacy_pack77_packtext77
     module procedure ftx_pack77_packtext77
  end interface legacy_pack77_packtext77

  interface legacy_pack77_unpacktext77
     module procedure ftx_pack77_unpacktext77
  end interface legacy_pack77_unpacktext77

  interface
     subroutine legacy_pack77_reset_context_c() bind(C,name="legacy_pack77_reset_context_c")
     end subroutine legacy_pack77_reset_context_c

     subroutine legacy_pack77_set_context_c(mycall,hiscall) bind(C,name="legacy_pack77_set_context_c")
       use, intrinsic :: iso_c_binding, only: c_char
       character(kind=c_char), intent(in) :: mycall(13)
       character(kind=c_char), intent(in) :: hiscall(13)
     end subroutine legacy_pack77_set_context_c

     subroutine legacy_pack77_set_dxbase_c(value) bind(C,name="legacy_pack77_set_dxbase_c")
       use, intrinsic :: iso_c_binding, only: c_char
       character(kind=c_char), intent(in) :: value(6)
     end subroutine legacy_pack77_set_dxbase_c

     subroutine legacy_pack77_pack_c(msg0,i3,n3,c77,msgsent,success,received) bind(C,name="legacy_pack77_pack_c")
       use, intrinsic :: iso_c_binding, only: c_bool, c_char, c_int
       character(kind=c_char), intent(in) :: msg0(37)
       integer(c_int), intent(out) :: i3
       integer(c_int), intent(out) :: n3
       character(kind=c_char), intent(out) :: c77(77)
       character(kind=c_char), intent(out) :: msgsent(37)
       logical(c_bool), intent(out) :: success
       integer(c_int), value :: received
     end subroutine legacy_pack77_pack_c

     subroutine legacy_pack77_unpack_c(c77,received,msgsent,success) bind(C,name="legacy_pack77_unpack_c")
       use, intrinsic :: iso_c_binding, only: c_bool, c_char, c_int
       character(kind=c_char), intent(in) :: c77(77)
       integer(c_int), value :: received
       character(kind=c_char), intent(out) :: msgsent(37)
       logical(c_bool), intent(out) :: success
     end subroutine legacy_pack77_unpack_c

     subroutine legacy_pack77_save_hash_call_c(c13,n10,n12,n22) bind(C,name="legacy_pack77_save_hash_call_c")
       use, intrinsic :: iso_c_binding, only: c_char, c_int
       character(kind=c_char), intent(in) :: c13(13)
       integer(c_int), intent(out) :: n10
       integer(c_int), intent(out) :: n12
       integer(c_int), intent(out) :: n22
     end subroutine legacy_pack77_save_hash_call_c

     subroutine legacy_pack77_pack28_c(c13,n28,success) bind(C,name="legacy_pack77_pack28_c")
       use, intrinsic :: iso_c_binding, only: c_bool, c_char, c_int
       character(kind=c_char), intent(in) :: c13(13)
       integer(c_int), intent(out) :: n28
       logical(c_bool), intent(out) :: success
     end subroutine legacy_pack77_pack28_c

     subroutine legacy_pack77_unpack28_c(n28,c13,success) bind(C,name="legacy_pack77_unpack28_c")
       use, intrinsic :: iso_c_binding, only: c_bool, c_char, c_int
       integer(c_int), value :: n28
       character(kind=c_char), intent(out) :: c13(13)
       logical(c_bool), intent(out) :: success
     end subroutine legacy_pack77_unpack28_c

     subroutine legacy_pack77_split77_c(msg,nwords,nw,w) bind(C,name="legacy_pack77_split77_c")
       use, intrinsic :: iso_c_binding, only: c_char, c_int
       character(kind=c_char), intent(in) :: msg(37)
       integer(c_int), intent(out) :: nwords
       integer(c_int), intent(out) :: nw(19)
       character(kind=c_char), intent(out) :: w(13,19)
     end subroutine legacy_pack77_split77_c

     subroutine legacy_pack77_packtext77_c(c13,c71,success) bind(C,name="legacy_pack77_packtext77_c")
       use, intrinsic :: iso_c_binding, only: c_bool, c_char
       character(kind=c_char), intent(in) :: c13(13)
       character(kind=c_char), intent(out) :: c71(71)
       logical(c_bool), intent(out) :: success
     end subroutine legacy_pack77_packtext77_c

     subroutine legacy_pack77_unpacktext77_c(c71,c13,success) bind(C,name="legacy_pack77_unpacktext77_c")
       use, intrinsic :: iso_c_binding, only: c_bool, c_char
       character(kind=c_char), intent(in) :: c71(71)
       character(kind=c_char), intent(out) :: c13(13)
       logical(c_bool), intent(out) :: success
     end subroutine legacy_pack77_unpacktext77_c
  end interface

contains

  subroutine to_c_fixed(src, dst)
    character(len=*), intent(in) :: src
    character(kind=c_char), intent(out) :: dst(:)
    integer :: i, n

    do i = 1, size(dst)
       dst(i) = transfer(' ', dst(i))
    end do
    n = min(size(dst), len(src))
    do i = 1, n
       dst(i) = transfer(src(i:i), dst(i))
    end do
  end subroutine to_c_fixed

  subroutine from_c_fixed(src, dst)
    character(kind=c_char), intent(in) :: src(:)
    character(len=*), intent(out) :: dst
    integer :: i, n

    dst = ' '
    n = min(size(src), len(dst))
    do i = 1, n
       dst(i:i) = transfer(src(i), dst(i:i))
    end do
  end subroutine from_c_fixed

  subroutine ftx_pack77_reset_context()
    call legacy_pack77_reset_context_c()
  end subroutine ftx_pack77_reset_context

  subroutine ftx_pack77_set_context(mycall, hiscall)
    character(len=*), intent(in) :: mycall, hiscall
    character(kind=c_char) :: mycall_c(13), hiscall_c(13)

    call to_c_fixed(adjustl(mycall), mycall_c)
    call to_c_fixed(adjustl(hiscall), hiscall_c)
    call legacy_pack77_set_context_c(mycall_c, hiscall_c)
  end subroutine ftx_pack77_set_context

  subroutine ftx_pack77_set_dxbase(value)
    character(len=*), intent(in) :: value
    character(kind=c_char) :: value_c(6)

    call to_c_fixed(adjustl(value), value_c)
    call legacy_pack77_set_dxbase_c(value_c)
  end subroutine ftx_pack77_set_dxbase

  subroutine ftx_pack77_pack(msg0, i3, n3, c77, msgsent, success, received)
    character(len=*), intent(in) :: msg0
    integer, intent(inout) :: i3, n3
    character(len=*), intent(out) :: c77
    character(len=*), intent(out) :: msgsent
    logical, intent(out) :: success
    integer, intent(in), optional :: received
    character(kind=c_char) :: msg0_c(37), c77_c(77), msgsent_c(37)
    integer(c_int) :: i3_c, n3_c, received_c
    logical(c_bool) :: success_c

    received_c = 1_c_int
    if (present(received)) received_c = received
    call to_c_fixed(msg0, msg0_c)
    call legacy_pack77_pack_c(msg0_c, i3_c, n3_c, c77_c, msgsent_c, success_c, received_c)
    call from_c_fixed(c77_c, c77)
    call from_c_fixed(msgsent_c, msgsent)
    i3 = i3_c
    n3 = n3_c
    success = success_c
  end subroutine ftx_pack77_pack

  subroutine ftx_pack77_unpack(c77, received, msgsent, success)
    character(len=*), intent(in) :: c77
    integer, intent(in) :: received
    character(len=*), intent(out) :: msgsent
    logical, intent(out) :: success
    character(kind=c_char) :: c77_c(77), msgsent_c(37)
    integer(c_int) :: received_c
    logical(c_bool) :: success_c

    call to_c_fixed(c77, c77_c)
    received_c = received
    call legacy_pack77_unpack_c(c77_c, received_c, msgsent_c, success_c)
    call from_c_fixed(msgsent_c, msgsent)
    success = success_c
  end subroutine ftx_pack77_unpack

  subroutine ftx_pack77_save_hash_call(c13, n10, n12, n22)
    character(len=*), intent(in) :: c13
    integer, intent(out) :: n10, n12, n22
    character(kind=c_char) :: c13_c(13)
    integer(c_int) :: n10_c, n12_c, n22_c

    call to_c_fixed(c13, c13_c)
    call legacy_pack77_save_hash_call_c(c13_c, n10_c, n12_c, n22_c)
    n10 = n10_c
    n12 = n12_c
    n22 = n22_c
  end subroutine ftx_pack77_save_hash_call

  integer function ftx_pack77_hash_call(c13, m)
    character(len=*), intent(in) :: c13
    integer, intent(in) :: m
    integer :: n10, n12, n22

    call ftx_pack77_save_hash_call(c13, n10, n12, n22)
    select case (m)
    case (10)
      ftx_pack77_hash_call = n10
    case (12)
      ftx_pack77_hash_call = n12
    case (22)
      ftx_pack77_hash_call = n22
    case default
      ftx_pack77_hash_call = -1
    end select
  end function ftx_pack77_hash_call

  subroutine ftx_pack77_pack28(c13, n28)
    character(len=*), intent(in) :: c13
    integer, intent(out) :: n28
    character(kind=c_char) :: c13_c(13)
    integer(c_int) :: n28_c
    logical(c_bool) :: success_c

    call to_c_fixed(c13, c13_c)
    call legacy_pack77_pack28_c(c13_c, n28_c, success_c)
    n28 = n28_c
  end subroutine ftx_pack77_pack28

  subroutine ftx_pack77_unpack28(n28, c13, success)
    integer, intent(in) :: n28
    character(len=*), intent(out) :: c13
    logical, intent(out) :: success
    character(kind=c_char) :: c13_c(13)
    integer(c_int) :: n28_c
    logical(c_bool) :: success_c

    n28_c = n28
    call legacy_pack77_unpack28_c(n28_c, c13_c, success_c)
    call from_c_fixed(c13_c, c13)
    success = success_c
  end subroutine ftx_pack77_unpack28

  subroutine ftx_pack77_split77(msg, nwords, nw, w)
    character(len=*), intent(in) :: msg
    integer, intent(out) :: nwords
    integer, intent(out) :: nw(*)
    character(len=*), intent(out) :: w(*)
    character(kind=c_char) :: msg_c(37), w_c(13,19)
    integer(c_int) :: nwords_c, nw_c(19)
    integer :: i, j

    call to_c_fixed(msg, msg_c)
    call legacy_pack77_split77_c(msg_c, nwords_c, nw_c, w_c)
    nwords = nwords_c
    do i = 1, 19
       nw(i) = nw_c(i)
       w(i) = ' '
       do j = 1, min(len(w(i)), 13)
          w(i)(j:j) = transfer(w_c(j,i), w(i)(j:j))
       end do
    end do
  end subroutine ftx_pack77_split77

  subroutine ftx_pack77_packtext77(c13, c71)
    character(len=*), intent(in) :: c13
    character(len=*), intent(out) :: c71
    character(kind=c_char) :: c13_c(13), c71_c(71)
    logical(c_bool) :: success_c

    call to_c_fixed(c13, c13_c)
    call legacy_pack77_packtext77_c(c13_c, c71_c, success_c)
    call from_c_fixed(c71_c, c71)
  end subroutine ftx_pack77_packtext77

  subroutine ftx_pack77_unpacktext77(c71, c13)
    character(len=*), intent(in) :: c71
    character(len=*), intent(out) :: c13
    character(kind=c_char) :: c71_c(71), c13_c(13)
    logical(c_bool) :: success_c

    call to_c_fixed(c71, c71_c)
    call legacy_pack77_unpacktext77_c(c71_c, c13_c, success_c)
    call from_c_fixed(c13_c, c13)
  end subroutine ftx_pack77_unpacktext77

end module ftx_pack77_c_api
