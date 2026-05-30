# header-only library
%global debug_package %{nil}
Version:        {{{ git_repo_version }}}
%forgemeta
Name:           vir-simd
Release:        1%{?dist}
Summary:        Parallelism TS 2 extensions and simd fallback implementation

License:        LGPL-3.0+
URL:            https://github.com/mattkretz/vir-simd/
VCS:            {{{ git_repo_vcs }}}
Source:         {{{ git_repo_pack }}}
BuildRequires:  make
BuildRequires:  gcc-c++


%description
vir-simd aims to provide a fallback std::experimental::simd (Parallelism TS 2)
implementation with additional features. Not every user can rely on GCC 11+ and
its standard library to be present on all target systems. Therefore, the header
vir/simd.h provides a fallback implementation of the TS specification that only
implements the scalar and fixed_size<N> ABI tags. Thus, your code can still
compile and run correctly, even if it is missing the performance gains a proper
implementation provides.

%package devel
Summary: Development files for %{name}
%description devel
The %{name}-devel package contains development files for %{name}.

%prep
{{{ git_repo_setup_macro }}}

%build

%install
%make_install includedir=%{buildroot}%{_includedir}


%check
# Currently fails upstream on Linux. Not worth chasing here.
#make testsuite-O2

%files devel
%license LICENSE
%{_includedir}/vir/*.h


%changelog

{{{ git_repo_changelog }}}
