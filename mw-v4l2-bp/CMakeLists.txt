add_executable(mw_media_buf_test media_buf_test.c)

if(MW_SHARED_ENABLE)
	add_dependencies(mw_media_buf_test ${PROJECT_NAME}_shared)
	target_link_libraries(mw_media_buf_test ${PROJECT_NAME}_shared)
else()
	add_dependencies(mw_media_buf_test ${PROJECT_NAME}_static)
	target_link_libraries(mw_media_buf_test ${PROJECT_NAME}_static)
endif()


add_executable(mw_media_engine_test media_engine_test.c)

if(MW_SHARED_ENABLE)
	add_dependencies(mw_media_engine_test ${PROJECT_NAME}_shared)
	target_link_libraries(mw_media_engine_test ${PROJECT_NAME}_shared)
else()
	add_dependencies(mw_media_engine_test ${PROJECT_NAME}_static)
	target_link_libraries(mw_media_engine_test ${PROJECT_NAME}_static)
endif()


add_executable(mw_video_pattern_test video_pattern_test.c)

if(MW_SHARED_ENABLE)
	add_dependencies(mw_video_pattern_test ${PROJECT_NAME}_shared)
	target_link_libraries(mw_video_pattern_test ${PROJECT_NAME}_shared)
else()
	add_dependencies(mw_video_pattern_test ${PROJECT_NAME}_static)
	target_link_libraries(mw_video_pattern_test ${PROJECT_NAME}_static)
endif()

if(MW_PRO_CONVERT_KVM)

add_executable(mw_pro_convert_kvm_video_capture_test pro_convert_kvm_video_capture_test.c)

if(MW_SHARED_ENABLE)
	add_dependencies(mw_pro_convert_kvm_video_capture_test ${PROJECT_NAME}_shared)
	target_link_libraries(mw_pro_convert_kvm_video_capture_test ${PROJECT_NAME}_shared)
else()
	add_dependencies(mw_pro_convert_kvm_video_capture_test ${PROJECT_NAME}_static)
	target_link_libraries(mw_pro_convert_kvm_video_capture_test ${PROJECT_NAME}_static)
endif()

add_executable(mw_kvm_pl_cap_enc_test kvm_pl_cap_enc_test.c)

if(MW_SHARED_ENABLE)
	add_dependencies(mw_kvm_pl_cap_enc_test ${PROJECT_NAME}_shared)
	target_link_libraries(mw_kvm_pl_cap_enc_test ${PROJECT_NAME}_shared)
else()
	add_dependencies(mw_kvm_pl_cap_enc_test ${PROJECT_NAME}_static)
	target_link_libraries(mw_kvm_pl_cap_enc_test ${PROJECT_NAME}_static)
endif()

add_executable(mw_kvm_recv_dec_test kvm_recv_dec_render_test.c)

if(MW_SHARED_ENABLE)
	add_dependencies(mw_kvm_recv_dec_test ${PROJECT_NAME}_shared)
	target_link_libraries(mw_kvm_recv_dec_test ${PROJECT_NAME}_shared)
else()
	add_dependencies(mw_kvm_recv_dec_test ${PROJECT_NAME}_static)
	target_link_libraries(mw_kvm_recv_dec_test ${PROJECT_NAME}_static)
endif()


add_executable(mw_hdmi_output_test hdmi_output_test.c)

if(MW_SHARED_ENABLE)
	add_dependencies(mw_hdmi_output_test ${PROJECT_NAME}_shared)
	target_link_libraries(mw_hdmi_output_test ${PROJECT_NAME}_shared)
else()
	add_dependencies(mw_hdmi_output_test ${PROJECT_NAME}_static)
	target_link_libraries(mw_hdmi_output_test ${PROJECT_NAME}_static)
endif()

endif()

add_executable(mw_llmt_send_test llmt_send_test.c)

if(MW_SHARED_ENABLE)
	add_dependencies(mw_llmt_send_test ${PROJECT_NAME}_shared)
	target_link_libraries(mw_llmt_send_test ${PROJECT_NAME}_shared)
else()
	add_dependencies(mw_llmt_send_test ${PROJECT_NAME}_static)
	target_link_libraries(mw_llmt_send_test ${PROJECT_NAME}_static)
endif()

add_executable(mw_llmt_recv_test llmt_recv_test.c)

if(MW_SHARED_ENABLE)
	add_dependencies(mw_llmt_recv_test ${PROJECT_NAME}_shared)
	target_link_libraries(mw_llmt_recv_test ${PROJECT_NAME}_shared)
else()
	add_dependencies(mw_llmt_recv_test ${PROJECT_NAME}_static)
	target_link_libraries(mw_llmt_recv_test ${PROJECT_NAME}_static)
endif()

add_executable(mw_llmt_rendezvousd llmt_rendezvousd.c)

if(MW_SHARED_ENABLE)
	add_dependencies(mw_llmt_rendezvousd ${PROJECT_NAME}_shared)
	target_link_libraries(mw_llmt_rendezvousd ${PROJECT_NAME}_shared)
else()
	add_dependencies(mw_llmt_rendezvousd ${PROJECT_NAME}_static)
	target_link_libraries(mw_llmt_rendezvousd ${PROJECT_NAME}_static)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBNL3 libnl-genl-3.0)
add_executable(mw_wifi_send_test wifi_send_test.c)
target_include_directories(mw_wifi_send_test SYSTEM PUBLIC ${LIBNL3_INCLUDE_DIRS})

if(MW_SHARED_ENABLE)
	add_dependencies(mw_wifi_send_test ${PROJECT_NAME}_shared)
	target_link_libraries(mw_wifi_send_test ${PROJECT_NAME}_shared ${LIBNL3_LIBRARIES})
else()
	add_dependencies(mw_wifi_send_test ${PROJECT_NAME}_static)
	target_link_libraries(mw_wifi_send_test ${PROJECT_NAME}_static ${LIBNL3_LIBRARIES})
endif()

add_executable(mw_wifi_recv_test wifi_recv_test.c)
target_include_directories(mw_wifi_recv_test SYSTEM PUBLIC ${LIBNL3_INCLUDE_DIRS})

if(MW_SHARED_ENABLE)
	add_dependencies(mw_wifi_recv_test ${PROJECT_NAME}_shared)
	target_link_libraries(mw_wifi_recv_test ${PROJECT_NAME}_shared ${LIBNL3_LIBRARIES})
else()
	add_dependencies(mw_wifi_recv_test ${PROJECT_NAME}_static)
	target_link_libraries(mw_wifi_recv_test ${PROJECT_NAME}_static ${LIBNL3_LIBRARIES})
endif()

add_executable(mw_v4l2_ctrl_test v4l2_ctrl_test.c)

if(MW_SHARED_ENABLE)
	add_dependencies(mw_v4l2_ctrl_test ${PROJECT_NAME}_shared)
	target_link_libraries(mw_v4l2_ctrl_test ${PROJECT_NAME}_shared)
else()
	add_dependencies(mw_v4l2_ctrl_test ${PROJECT_NAME}_static)
	target_link_libraries(mw_v4l2_ctrl_test ${PROJECT_NAME}_static)
endif()

