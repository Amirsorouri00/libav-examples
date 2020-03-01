usage:
	echo "make fetch_small_bunny_video && make run_hello"

all: clean fetch_bbb_video make_hello run_hello make_remuxing run_remuxing_ts run_remuxing_fragmented_mp4 make_transcoding
.PHONY: all

clean:
	@rm -rf ./build/*

fetch_small_bunny_video:
	./fetch_bbb_video.sh

make_hello: clean
	  gcc -g -Wall 0_hello_filter.c \
	  -lavcodec -lavformat -lavfilter -lavdevice -lswresample -lswscale -lavutil \
	  -o hello

run_hello: make_hello
	./hello small_bunny_1080p_60fps.mp4

#make_remuxing: clean
#	  gcc 2_remuxing.c \
#	  -lavcodec -lavformat -lavfilter -lavdevice -lswresample -lswscale -lavutil \
#	  -o remuxing
#
#run_remuxing_ts: make_remuxing
#	remuxing small_bunny_1080p_60fps.mp4 remuxed_small_bunny_1080p_60fps.ts
#
#run_remuxing_fragmented_mp4: make_remuxing
#	remuxing small_bunny_1080p_60fps.mp4 fragmented_small_bunny_1080p_60fps.mp4 fragmented
#
#make_transcoding: clean
#	  gcc -g -Wall 3_transcoding.c video_debugging.c \
#	  -lavcodec -lavformat -lavfilter -lavdevice -lswresample -lswscale -lavutil \
#	  -o 3_transcoding
#
#run_transcoding: make_transcoding
#	small_bunny_1080p_60fps.mp4 bunny_1s_gop.mp4
