module wsjtx_wav_catalog
  implicit none

  integer, parameter, public :: NBANDS = 23
  integer, parameter, public :: NMODES = 13
  integer, parameter, public :: NPERIODS = 8

  character(len=8), parameter, public :: WSJTX_MODES(NMODES) = (/ &
       'Echo    ', 'FSK441  ', 'ISCAT   ', 'JT4     ', 'JT65    ', &
       'JT6M    ', 'JT9     ', 'JT9+JT65', 'JTMS    ', 'JTMSK   ', &
       'WSPR    ', 'FT8     ', 'FT2     ' /)

  character(len=6), parameter, public :: WSJTX_BANDS(NBANDS) = (/ &
       '2190m ', '630m  ', '160m  ', '80m   ', '60m   ', '40m   ', &
       '30m   ', '20m   ', '17m   ', '15m   ', '12m   ', '10m   ', &
       '6m    ', '2m    ', '1.25m ', '70cm  ', '33cm  ', '23cm  ', &
       '13cm  ', '9cm   ', '6cm   ', '3cm   ', '1.25cm' /)

  integer, parameter, public :: WSJTX_PERIODS(NPERIODS) = (/ &
       5, 10, 15, 30, 60, 120, 900, 0 /)
end module wsjtx_wav_catalog
