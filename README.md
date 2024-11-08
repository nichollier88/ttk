UI toolkit for iPodLinux applications.  

---

This is a component of iPodLinux that has been split off into it's own separate project.  
This project is no longer actively maintained and has been mirrored for archival purposes.  

The iPodLinux project's full source code tree: https://github.com/iPodLinux/iPodLinux-SVN  
The original SourceForge project: http://sourceforge.net/projects/ipodlinux/  
The (now dead) website: http://ipodlinux.org/  

All files are licensed under GNU General Public License v2.0 unless otherwise specified.  
http://www.gnu.org/licenses/gpl-2.0.html 

# Dependencies

```
flex libsdl1.2-dev libsdl-image1.2-dev
```

# Build

```
make NOIPOD=1 NOHDOG=1 all -j
```