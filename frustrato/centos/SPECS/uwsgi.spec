# Version
%global majornumber 2
%global minornumber 0
%global releasenumber 10

Name:           uwsgi
Version:        %{majornumber}.%{minornumber}.%{releasenumber}
Release:        1%{?dist}
Summary:        Fast, self-healing, application container server
Group:          System Environment/Daemons
License:        GPLv2 with exceptions
URL:            https://github.com/unbit/uwsgi
Source0:        http://projects.unbit.it/downloads/%{name}-%{version}.tar.gz

%description
uWSGI is a fast (pure C), self-healing, developer/sysadmin-friendly
application container server.  Born as a WSGI-only server, over time it has
evolved in a complete stack for networked/clustered web applications,
implementing message/object passing, caching, RPC and process management.
It uses the uwsgi (all lowercase, already included by default in the Nginx
and Cherokee releases) protocol for all the networking/interprocess
communications.  Can be run in preforking mode, threaded,
asynchronous/evented and supports various form of green threads/co-routine
(like uGreen and Fiber).  Sysadmin will love it as it can be configured via
command line, environment variables, xml, .ini and yaml files and via LDAP.
Being fully modular can use tons of different technology on top of the same
core.

%prep
%setup -n %{name}

%build
python setup.py build

%install
python setup.py install --root %{buildroot}

%files
%{python_sitelib}/uWSGI-%{majornumber}.%{minornumber}.%{releasenumber}-py2.7.egg-info
%{python_sitelib}/%{name}decorators.py*
%{_bindir}/%{name}

