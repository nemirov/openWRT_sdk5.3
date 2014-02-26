openWRT_sdk5.3
==============

OpenWRT for SDK 5.3 Triada

Компиляция пакета для OpenWRT:
Информация взята из http://dipcore.com/?p=224
1. svn co svn://svn.openwrt.org/openwrt/branches/backfire openwrt
2. cd openwrt
3. ./scripts/feeds update -a && ./scripts/feeds install -a
4. make prereq && make tools/install && make toolchain/install
5. cd package/
6. git clone https://github.com/nemirov/openWRT_sdk5.3 sdk
7. cd ../
8. make menuconfig
9. Utilities  ---> <M> sdk
8. make V=99

Сокмпилированный пакет под выбранную платформу находится в /bin/x86(платформа)/packages/sdk_x.x-x_x86.ipk

