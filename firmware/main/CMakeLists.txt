file(GLOB EPAPER_C_SRC epaper/*.c epaper/font/*.c epaper/bitmap/*.c)

set(
	COMPONENT_SRCS
	main.c
	audio.c
	http_client.c
	http_server.c
	gattc.c
	gatts.c
	wifi.c
	${EPAPER_C_SRC}
)
set(COMPONENT_ADD_INCLUDEDIRS "" "epaper")
set(COMPONENT_EMBED_TXTFILES)

register_component()
