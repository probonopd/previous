#
# RPM spec file for Previous
#
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#

BuildRequires: bash coreutils cpio cpp diffutils file filesystem findutils glibc glibc-devel grep groff gzip libgcc m4 make man mktemp patch readline sed tar unzip util-linux zlib zlib-devel SDL SDL-devel autoconf binutils gcc libtool rpm

Name:         previous
URL:          http://previous.alternative-system.com
License:      GPL
Group:        System/Emulators/Other
Autoreqprov:  on
Version:      0.4
Release:      1
Summary:      a NeXT computer emulator based on hatari emulator source code
Source:       %{name}-%{version}.tar.gz
#Patch:        %{name}-%{version}.dif
BuildRoot:    %{_tmppath}/%{name}-%{version}-build
Prefix:       /usr

%description
Previous is an emulator for NeXT computer "black" hardware

%prep
%setup
#%patch

%build
# LDFLAGS="-static" LIBS=`sdl-config --static-libs` \
CFLAGS="-O3 -fomit-frame-pointer" \
 ./configure --prefix=/usr --sysconfdir=/etc
make

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/bin/previous
/usr/share/previous
%doc %_mandir/man1/previous.1*
%dir %_docdir/%{name}
%_docdir/%{name}/*.txt
%_docdir/%{name}/*.html
%dir %_docdir/%{name}/images
%_docdir/%{name}/images/*.png

%changelog -n previous



