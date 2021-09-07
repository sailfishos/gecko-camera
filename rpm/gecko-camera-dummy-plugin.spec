Name:           gecko-camera-dummy-plugin
Summary:        A dummy camera plugin for gecko-camera
Version:        0.1
Release:        1
License:        LGPLv2+
URL:            https://github.com/sailfishos/gecko-camera
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  meson
BuildRequires:  pkgconfig(geckocamera)
Requires:       gecko-camera

%description
A library to simplify video capture, dummy camera plugin.

%prep
%autosetup

%build
%meson -Dbuild-tests=false -Dbuild-examples=false -Dbuild-devel=false -Dbuild-droid-plugin=false -Dbuild-dummy-plugin=true
meson rewrite kwargs set project / version %{version}-%{release}
%meson_build

%install
%meson_install

%files
%license LICENSE
%{_libdir}/gecko-camera/plugins/libgeckocamera-dummy.so
