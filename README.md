Brand new pintos for Operating Systems and Lab (CS330), KAIST, by Youngjin Kwon.

The manual is available at https://casys-kaist.github.io/pintos-kaist/.

# Environment Setting
### 1. Ubuntu 16.04 LTS download
   https://gist.github.com/xynova/87beae35688476efb2ee290d3926f5bb

### 1-1. vscode download
   https://update.code.visualstudio.com/1.85.2/win32-x64-user/stable

### 2. clone pintos repository
   
   git clone https://github.com/casys-kaist/pintos-kaist
   
### 3. executing install.sh

   cd pintos-kaist
   source ./install.sh
   
### 4. add activate to .bashrc
   
   echo "source ~/pintos-kaist/activate" >> ~/.bashrc
   source ~/.bashrc

### 5. Pintos permission denied error

   chmod +x utils/pintos
