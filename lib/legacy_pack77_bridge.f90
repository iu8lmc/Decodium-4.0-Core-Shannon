module legacy_pack77_bridge

  use ftx_pack77_c_api, only: MAXRECENT,                                 &
       legacy_pack77_reset_context, legacy_pack77_set_context,           &
       legacy_pack77_pack, legacy_pack77_unpack, legacy_pack77_save_hash_call, &
       legacy_pack77_set_dxbase, legacy_pack77_pack28,                  &
       legacy_pack77_unpack28, legacy_pack77_split77,                   &
       legacy_pack77_packtext77, legacy_pack77_unpacktext77
  implicit none
  public

end module legacy_pack77_bridge
