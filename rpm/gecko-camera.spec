Name:           gecko-camera
Summary:        A plugin-based library for Gecko to simplify video capture
Version:        0.1
Release:        1
License:        LGPLv2+
URL:            https://github.com/sailfishos/gecko-camera
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  meson
Recommends:     gecko-camera-droid-plugin

%description
%{summary}.

%package        devel
Summary:        gecko-camera development headers
Requires:       gecko-camera = %{version}-%{release}

%description    devel
%{summary}.

%package        example
Summary:        gecko-camera example
Requires:       gecko-camera = %{version}-%{release}

%description    example
%{summary}.

%prep
%autosetup

%build
%meson -Dbuild-tests=false -Dbuild-plugins=false
meson rewrite kwargs set project / version %{version}-%{release}
%meson_build

%install
%meson_install

%files
%license LICENSE
%{_libdir}/libgeckocamera.so.*

%files devel
%{_includedir}/gecko-camera/*.h
%{_libdir}/libgeckocamera.so
%{_libdir}/pkgconfig/geckocamera.pc

%files example
%{_datadir}/%{name}/geckocamera-example
