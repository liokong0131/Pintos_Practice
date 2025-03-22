Brand new pintos for Operating Systems and Lab (CS330), KAIST, by Youngjin Kwon.

The manual is available at https://casys-kaist.github.io/pintos-kaist/.

# Environment Setting
## 1. Ubuntu 16.04 LTS download
   https://gist.github.com/xynova/87beae35688476efb2ee290d3926f5bb
   
## 2. gcc-7.4.0 download
####   1) essential package download
      '''
      sudo apt update
      sudo apt install -y build-essential manpages-dev flex bison \
          libgmp-dev libmpfr-dev libmpc-dev
      '''

####   2) extract gcc-7.4.0.tar.gz and set prerequisites
      '''
      wget http://ftp.gnu.org/gnu/gcc/gcc-7.4.0/gcc-7.4.0.tar.gz
      tar -xzf gcc-7.4.0.tar.gz
      cd gcc-7.4.0
      ./contrib/download_prerequisites
      cd ..
      '''

####   3) build
      '''
      mkdir gcc-build
      cd gcc-build
      ../gcc-7.4.0/configure --prefix=/usr/local/gcc-7.4.0 \
          --enable-languages=c,c++ --disable-multilib
      make -j$(nproc)
      sudo make install
      cd ..
      '''
####   4) clean up
      '''
      rm -rf gcc-7.4.0 gcc-build gcc-7.4.0.tar.gz
      '''

## 3. qemu-2.5.0 download
####   1) essential package download
      '''
      sudo apt update
      sudo apt install -y build-essential libglib2.0-dev libpixman-1-dev \
          zlib1g-dev libfdt-dev
      '''
####   2) python 2 download
      '''
      apt-get update
      apt-get install -y python
      '''
      
####   3) extract qemu-2.5.0.tar.xz
      '''
      wget https://download.qemu.org/qemu-2.5.0.tar.xz
      tar -xJf qemu-2.5.0.tar.xz
      '''
      
####   4) build
      '''
      mkdir qemu-build
      cd qemu-build
      ../qemu-2.5.0/configure --prefix=/usr/local/qemu-2.5.0 --target-list=i386-softmmu
      make -j$(nproc)
      sudo make install
      cd ..
      '''
      
####   5) clean up
      '''
      rm -rf qemu-2.5.0 qemu-build qemu-2.5.0.tar.xz
      '''
      
