SECTIONS
{
  .mxc_epdc_fb_damage_ko : {
    mxc_epdc_fb_damage_ko_begin = . ;
    build/mxc_epdc_fb_damage.bin(.data)
    mxc_epdc_fb_damage_ko_end = . ;
  }
}
