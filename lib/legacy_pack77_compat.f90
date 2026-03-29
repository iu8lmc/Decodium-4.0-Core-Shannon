module legacy_pack77_compat

  use ftx_pack77_c_api, only: MAXRECENT, legacy_pack77_reset_context, &
       legacy_pack77_pack, legacy_pack77_unpack, legacy_pack77_pack28,    &
       legacy_pack77_unpack28, legacy_pack77_save_hash_call,              &
       legacy_pack77_split77
  implicit none
  private

  public :: MAXRECENT
  public :: pack77
  public :: unpack77
  public :: pack28
  public :: unpack28
  public :: save_hash_call
  public :: split77
  public :: ihashcall

contains

  subroutine pack77(msg0, i3, n3, c77)
    character(len=*), intent(in) :: msg0
    integer, intent(inout) :: i3, n3
    character(len=*), intent(out) :: c77
    character(len=37) :: msgsent
    logical :: success

    call legacy_pack77_reset_context()
    call legacy_pack77_pack(msg0, i3, n3, c77, msgsent, success, 0)
  end subroutine pack77

  subroutine unpack77(c77, received, msgsent, success)
    character(len=*), intent(in) :: c77
    integer, intent(in) :: received
    character(len=*), intent(out) :: msgsent
    logical, intent(out) :: success

    call legacy_pack77_unpack(c77, received, msgsent, success)
  end subroutine unpack77

  subroutine pack28(c13, n28)
    character(len=*), intent(in) :: c13
    integer, intent(out) :: n28

    call legacy_pack77_pack28(c13, n28)
  end subroutine pack28

  subroutine unpack28(n28, c13, success)
    integer, intent(in) :: n28
    character(len=*), intent(out) :: c13
    logical, intent(out) :: success

    call legacy_pack77_unpack28(n28, c13, success)
  end subroutine unpack28

  subroutine save_hash_call(c13, n10, n12, n22)
    character(len=*), intent(in) :: c13
    integer, intent(out) :: n10, n12, n22

    call legacy_pack77_save_hash_call(c13, n10, n12, n22)
  end subroutine save_hash_call

  subroutine split77(msg, nwords, nw, w)
    character(len=*), intent(in) :: msg
    integer, intent(out) :: nwords
    integer, intent(out) :: nw(*)
    character(len=*), intent(out) :: w(*)

    call legacy_pack77_split77(msg, nwords, nw, w)
  end subroutine split77

  integer function ihashcall(c13, m)
    character(len=*), intent(in) :: c13
    integer, intent(in) :: m
    integer :: n10, n12, n22

    call legacy_pack77_reset_context()
    call legacy_pack77_save_hash_call(c13, n10, n12, n22)
    select case (m)
    case (10)
      ihashcall = n10
    case (12)
      ihashcall = n12
    case (22)
      ihashcall = n22
    case default
      ihashcall = -1
    end select
  end function ihashcall

end module legacy_pack77_compat
