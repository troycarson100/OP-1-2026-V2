# Re-signs the built Standalone .app with a stable self-signed identity (when one is installed) so
# macOS TCC keeps granted folder-access permissions across rebuilds. The default JUCE build leaves
# the app ad-hoc signed, which TCC keys by the exact binary hash -> every rebuild looks like a new
# app and the OS re-prompts for Desktop/Downloads/external-drive access. A stable signature fixes
# that. No-op (with a hint) when the identity is missing, so the build never fails on a fresh clone.
#
# Invoked via -P with -DSCULPT_APP=<app bundle> -DSCULPT_ID=<identity name>.

if(NOT EXISTS "${SCULPT_APP}")
    return()
endif()

# Untrusted self-signed certs are still usable by codesign but do NOT appear under
# `find-identity -v`, so query without -v.
execute_process(
    COMMAND security find-identity -p codesigning
    OUTPUT_VARIABLE _ids
    ERROR_QUIET)

if(_ids MATCHES "${SCULPT_ID}")
    message(STATUS "Codesign: signing ${SCULPT_APP} with '${SCULPT_ID}' (stable macOS file permissions).")
    # Strip extended attributes (Finder info / resource forks, e.g. from iCloud-synced folders);
    # codesign refuses to sign items that carry them. No --deep: a standalone has no nested code,
    # and --deep trips over the bundle's Finder-info xattr.
    execute_process(COMMAND xattr -cr "${SCULPT_APP}" ERROR_QUIET)
    execute_process(
        COMMAND codesign --force --sign "${SCULPT_ID}" "${SCULPT_APP}"
        RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(WARNING "Codesign: signing failed (${_rc}); app stays ad-hoc and macOS will keep re-prompting.")
    endif()
else()
    message(STATUS "Codesign: identity '${SCULPT_ID}' not found - app stays ad-hoc. "
                   "Run scripts/setup-macos-codesign.sh once so macOS stops re-prompting for folder access.")
endif()
