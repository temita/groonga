# release: update-latest-release (commit) tag

LOCALES = ja

ACLOCAL_AMFLAGS = $$ACLOCAL_ARGS -I .
AUTOMAKE_OPTIONS = 1.9.6
SUBDIRS =					\
	include					\
	lib					\
	plugins					\
	src					\
	examples				\
	test					\
	packages				\
	data					\
	tools					\
	doc
#dist_data_DATA =
EXTRA_DIST = bindings version-gen.sh

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = groonga.pc

.PHONY: FORCE

version.sh: FORCE
	@$(SHELL_PATH) $(srcdir)/version-gen.sh

include $(srcdir)/version.sh

dist-hook:
	echo "$(GROONGA_VERSION)" > $(distdir)/version
	cd $(top_srcdir) && release/gitlog2changelog.py > $(distdir)/ChangeLog

benchmark:
	cd test/benchmark && $(MAKE) $(AM_MAKEFLAGS) benchmark

tag:
	cd $(top_srcdir) && git tag v$(VERSION) -a -m 'groonga $(VERSION)!!!'

echo-version:
	@echo $(VERSION)

update-latest-release:
	@if test -z "$(OLD_RELEASE)"; then	\
	  echo "\$$(OLD_RELEASE) is missing";	\
	  exit 1;				\
	fi
	@if test -z "$(OLD_RELEASE_DATE)"; then		\
	  echo "\$$(OLD_RELEASE_DATE) is missing";	\
	  exit 1;					\
	fi
	@if test -z "$(NEW_RELEASE_DATE)"; then		\
	  echo "\$$(NEW_RELEASE_DATE) is missing";	\
	  exit 1;					\
	fi
	cd $(top_srcdir) && test -d "misc" ||					\
	  svn export --force							\
	  https://cutter.svn.sourceforge.net/svnroot/cutter/cutter/trunk/misc
	cd $(top_srcdir) &&					\
	  misc/update-latest-release.rb				\
	  $(PACKAGE) $(OLD_RELEASE) $(OLD_RELEASE_DATE)		\
	  $(VERSION) $(NEW_RELEASE_DATE)			\
	  packages/rpm/fedora/groonga.spec.in			\
	  packages/rpm/centos/groonga.spec.in			\
	  packages/debian/changelog				\
	  $(GROONGA_GITHUB_COM_PATH)/index.html			\
	  $(GROONGA_GITHUB_COM_PATH)/ja/index.html

update-po:
	@for lang in $(LOCALES); do						\
	  (cd $(top_srcdir)/doc/locale/$$lang/LC_MESSAGES && make update)	\
	done

update-document:
	@if test -z "$(GROONGA_GITHUB_COM_PATH)"; then			\
	  echo "\$$(GROONGA_GITHUB_COM_PATH) is missing";		\
	  echo "add --with-groonga-github-com-path in configure";	\
	  exit 1;							\
	fi
	rm -rf tmp-doc
	mkdir tmp-doc
	(cd doc && $(MAKE) clean-html)
	(cd doc && $(MAKE) install docdir=$(abs_srcdir)/tmp-doc/install)
	ruby $(srcdir)/tools/prepare-sphinx-html.rb tmp-doc/install tmp-doc/dist
	rm -rf $(GROONGA_GITHUB_COM_PATH)/docs
	mv tmp-doc/dist/en $(GROONGA_GITHUB_COM_PATH)/docs
	for locale in `cd tmp-doc/dist; echo *`; do		\
	  dest_base_dir=$(GROONGA_GITHUB_COM_PATH)/$${locale};	\
	  mkdir -p $${dest_base_dir};				\
	  dest_dir=$${dest_base_dir}/docs;			\
	  rm -rf $${dest_dir};					\
	  mv tmp-doc/dist/$${locale} $${dest_dir};		\
	done
