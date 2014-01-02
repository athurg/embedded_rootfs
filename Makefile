#bsp.bin file which include 'stage0','u-boot' and 'uImage'
BSP_BIN    ?= ../tools/bsp.bin

# size of all images
APP_SIZE   ?= 0x1400000	#160 x 128 KiB(sec)
LOG_SIZE   ?= 0x600000	# 48 x 128 KiB(sec)
APP_OFFSET ?= 48	# APP_OFFSET = BSP_SIZE
LOG_OFFSET ?= 208	# LOG_OFFSET = BSP_SIZE + APP_SIZE

## STOP EDIT HERE ##
APP_FILES?=$(shell find app)
LOG_FILES =$(shell find log)

all:logfs.jffs2 appfs.jffs2

help:
	@echo "target:"
	@echo "	nor_flash:	Generate image for NorFlash"
	@echo "	clean:		Clean all *.jffs2 files"

nor_flash: ${BSP_BIN}
	@echo "Generate full app and log filesystem images"
	@mkfs.jffs2 -o appfs-full.jffs2 -r app -e 0x20000 -l --pad=${APP_SIZE}
	@mkfs.jffs2 -o logfs-full.jffs2 -r log -e 0x20000 -l --pad=${LOG_SIZE}
	@echo "Generate full NorFlash image"
	@dd if=${BSP_BIN} of=nor_flash.bin
	@dd bs=128K if=appfs-full.jffs2 of=nor_flash.bin seek=${APP_OFFSET}
	@dd bs=128K if=logfs-full.jffs2 of=nor_flash.bin seek=${LOG_OFFSET}

appfs.jffs2:${APP_FILES}
	@echo "生成应用程序分区镜像"
	@mkfs.jffs2 -o appfs.jffs2 -r app -e 0x20000 -l

logfs.jffs2:${LOG_FILES}
	@echo "生成日志分区镜像"
	@mkfs.jffs2 -o logfs.jffs2 -r log -e 0x20000 -l
	
clean:
	@echo "清理所有分区镜像"
	-@rm -rf *.jffs2

distclean: clean
rebuild:clean all
