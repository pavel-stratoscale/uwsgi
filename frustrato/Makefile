all: rpms

UWSGI = $(CURDIR)
FRUSTRATO = $(UWSGI)/frustrato
RPMBUILD = $(FRUSTRATO)/build

ORIG_SPEC = $(FRUSTRATO)/centos/SPECS/uwsgi.spec
SPEC = $(FRUSTRATO)/build/SPECS/uwsgi.spec

BUILDID ?= strato0001
UWSGI_HASH = $(shell git rev-parse --short=12 HEAD)
DIST = ".el7_0."$(BUILDID).$(UWSGI_HASH)

UWSGI_TAR = uwsgi.tar.gz
SOURCE0 = $(RPMBUILD)/SOURCES/$(UWSGI_TAR)

rpms: uwsgi

submit:
	solvent submitproduct rpms $(RPMBUILD)/RPMS/x86_64

approve:
	solvent approve --product=rpms

prepareForCleanBuild:
	sudo yum-builddep --assumeyes $(ORIG_SPEC)

clean:
	sudo rm -fr $(RPMBUILD)

sources: clean
	mkdir -p $(RPMBUILD)/SOURCES
	git archive --format=tar.gz --prefix=uwsgi/ --output=$(SOURCE0) HEAD 

spec: clean
	mkdir -p $(dir $(SPEC))
	sed -e "s/^Source0:.*/Source0: $(UWSGI_TAR)/" $(ORIG_SPEC) > $(SPEC)

uwsgi: sources spec
	rpmbuild -D "_topdir $(RPMBUILD)" -D "dist $(DIST)" -bb $(SPEC)

.PHONY: rpms
