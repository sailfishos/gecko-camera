Name:           gecko-camera-droid-plugin
Summary:        A droidmedia-based plugin for gecko-camera
Version:        0.1
Release:        1
License:        LGPLv2+
URL:            https://github.com/sailfishos/gecko-camera
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  meson
BuildRequires:  pkgconfig(geckocamera)
BuildRequires:  pkgconfig(droidmedia)
Requires:       gecko-camera
Requires:       droidmedia

%description
A library to simplify video capture, droidmedia-based plugin.

%prep
%autosetup

%build
%meson -Dbuild-tests=false -Dbuild-examples=false -Dbuild-devel=false -Dbuild-droid-plugin=true
meson rewrite kwargs set project / version %{version}-%{release}
%meson_build

%install
%meson_install

%files
%license LICENSE
%{_libdir}/gecko-camera/plugins/libgeckocamera-droid.so
