# **A314 emulation support**

## **About**

A314 is a trapdoor expansion board for Amiga 500 created by Niklas Ekström, It also uses a Raspberry Pi connected to bring interesting features to the Amiga. For more information about the board visit the [A314 Github](https://github.com/niklasekstrom/a314).

PiStorm provides the functionality of the A314 emulating the device on the Pi side.

## **Installation**

Enable the A314 emulation on your cfg file by uncommenting the line, by removing the #:

```
setvar a314
```

All A314 features will require that the file [`a314.device`](a314device/software-amiga/a314.device) is copied into DEVS: on the Amiga.

For your convenience all the Amiga required files are available on the PiStorm disk mounted by default. Make sure you have the following line uncommented on your cfg file:

```
setvar piscsi6 platforms/amiga/pistorm.hdf
```

Open the Amiga shell and run:

```
copy PISTORM:a314/a314.device DEVS:
```

***[OPTIONAL]*** If you need to change the location of your [`a314d.conf`](files_pi/a314d.conf) you can do it by uncommenting the line on your cfg file, by removing the #, the path can be relative to the folder the emulator is launched or absolute:

```
setvar a314_conf /home/pi/amiga-files/a314/files_pi/a314d.conf
```

---

### **a314fs**

a34is a file system that is mounted in AmigaDOS as a device, PI0:
The volume in PI0: is called PiDisk:, and is mapped to a directory in the RPi.

The default shared folder in the Pi is at `/home/pi/pistorm/data/a314-shared`

To enable it, on the Amiga you need to:

- Copy [`a314fs`](a314device/software-amiga/a314fs) to L:
- Append the contents of [`a314fs-mountlist`](a314device/software-amiga/a314fs-mountlist) to DEVS:Mountlist

This can be achieved by opening the Amiga shell and running the commands:

```
copy PISTORM:a314/a314fs L:
type PISTORM:a314/a314fs-mountlist >> DEVS:Mountlist
```

Now you can mount your shared folder with

```
mount PI0:
```

You can add the mount command into S:Startup-Sequence if you want it mounted automatically on boot.

**[OPTIONAL]** You can customise the location of your [`a314fs.conf`](files_pi/a314fs.conf) file by adding the parameter `-conf-file <conf-file-location>` to the `a314fs.py` python command line in the a314d.conf file. These paths can be relative to the folder where the emultor is running or absolute. E.g.:

```
a314fs		python3	/home/pi/pistorm/a314/files_pi/a314fs.py -conf-file /home/pi/amiga-files/config/a314fs.conf
```

The [`a314fs.conf`](files_pi/a314fs.conf) file allow you to define the location of your shared folder. This path can be relative to the folder where the emultor is running or absolute. It was supposed to allow you to give a different volume name and label on the amiga side, but this is currently not working. E.g.:

```
{
  "devices": {
    "PI0": {
      "volume": "PiDisk",
      "path": "/home/pi/amiga-files/shared"
    }
  }
}
```

---

### **pi command**

pi is a command that lets you invoke executables on the RPi from the Amiga side. For example, if your current working directory is on PiDisk: and you run "pi vc hello.c -o hello", then the vc program (the VBCC cross-compiler) is executed on the RPi with the given arguments. It will cross-compile “hello.c” into the Amiga executable file “hello”. The resulting binary is immediately accessible through the a314fs.

You may also launch Interactive applications using the pi command, such as "pi mc -a" which will run Midnight Commander. Running pi without any arguments is equivalent to "pi bash" and will present you with a bash prompt from the RPi.

To install the pi command just copy [`pi`](a314device/software-amiga/pi) to C: running the command:

```
copy PISTORM:a314/pi C:
```

---

### **ethernet**

A SANA-II driver that forwards Ethernet packets to the network interface of the RPi. Together with an Amiga TCP/IP stack this provides network access to the Amiga.

#### **On the Pi**:

- Install the pip3 command:
```
sudo apt install python3-pip
```

- Install python-pytun
```
sudo pip3 install python-pytun
```

- Copy the [`tap0`](files_pi/eth-config-pi/tap) to `/etc/network/interfaces.d/`:
```
sudo cp /home/pi/pistorm/a314/files_pi/eth-config-pi/tap0 /etc/network/interfaces.d/
```

- Add NAT rulest to forward the requests from the amiga into the wi-fi:
```
sudo iptables -t nat -A POSTROUTING -o wlan0 -j MASQUERADE  
sudo iptables -A FORWARD -i wlan0 -o tap0 -m state --state RELATED,ESTABLISHED -j ACCEPT  
sudo iptables -A FORWARD -i tap0 -o wlan0 -j ACCEPT
```

- Make the NAT rules persintent:
```
sudo apt install iptables-persistent
```
Agree to the IPv4 question, You can pick any answer for IPv6

- Enable IPv4 forwarding by editing /etc/sysctl.conf with:
```
sudo nano /etc/sysctl.conf
```
Uncomment the line, by removing the #:
```
net.ipv4.ip_forward=1
```

- Reboot the Pi.

#### **On the Amiga**:

- Copy the [`a314eth.device`](a314device/software-amiga/a314eth.device) to DEVS:
```
copy PISTORM:a314/a314eth.device DEVS;
```

- The ethernet emulated interface is installed and configured both on the Pi and the Amiga. You'll need a TCP/IP stack installed and configured on the Amiga to be abe to access the internet.

- **[Instructions for [Roadshow](http://roadshow.apc-tcp.de/index-en.php)**]
Roadshow is a proprietary software that costs €25.00. There's also a demo version that will shutdown after 15 minutes of usage.
    - Install Roadshow. If you already have Roadshow installed and configured on your HD image manually add the content of the following files instead of copying their contents over, or you will lose your current network setup.
    - Copy [`A314Eth`](software-amiga/eth-config-amiga/A314Eth) to DEVS:NetInterfaces/
    ```
     COPY PISTORM:a314/eth-config-amiga/A314Eth DEVS:NetInterfaces/
    ```
    - Copy [`routes`](software-amiga/eth-config-amiga/routes) to DEVS:Internet/
    ```
     COPY PISTORM:a314/eth-config-amiga/routes DEVS:Internet/
    ```
    - Copy [`name-resolution`](software-amiga/eth-config-amiga/name_resolution) to DEVS:Internet/
    - Congratulations, you have set the a314 networking on your pistorm. Check if everything worked on your Amiga Shell:
    ```
    ping www.google.com
    ```
- 

**[TODO: Instructions for [AmiTCP based stack]()]**


---

### PiAudio, RemoteWB, VideoPlayer are **not supported, and it's unlikely it will ever be as the PiStorm doesn't have full access to the Chip RAM bus.**

---