# GPOPT and ORCA code formatting

### Tools

[clang-format-style-options]: https://clang.llvm.org/docs/ClangFormatStyleOptions.html
[clang-format-style-options.10]: https://releases.llvm.org/10.0.0/tools/clang/docs/ClangFormatStyleOptions.html
[clang-format.10]: https://releases.llvm.org/10.0.0/tools/clang/docs/ClangFormat.html

1. We are using [clang-format](https://clang.llvm.org/docs/ClangFormat.html).

1. We use the current stable release, with an eye to good new options coming from [the next release][clang-format-style-options].
   As of writing we're using [release 10][clang-format.10],
   the configuration options are [documented here][clang-format-style-options.10].

1. As mentioned in the [style guide](StyleGuide.md), the format style is based on Google C++ style guide,
   but mixed in with Postgres formatting rules (break before braces, 4-space tabbing, etc)

1. We should use an explicit, complete (locked down) specification for the [.clang-format](.clang-format) file.

1. But our intent is better expressed as [well organized, commented yaml](clang-format.intent.yaml).
   We use a [simple script](../../../src/tools/fmt) to generate the complete config file from the intent file. For example, on my Linux laptop, I run:

   ```shell
   CLANG_FORMAT=clang-format-10 src/tools/fmt gen
   ```

   If the correct version of `clang-format` is installed as `clang-format` (as is the case in macOS), you can omit the environment variable override.

1. To check for formatting conformance, one can run

   ```shell
   src/tools/fmt chk
   ```

   It will succeed quietly (with return code 0) or point out the first few places that need to be formatted.

1. To wholesale format all of ORCA and GPOPT

   ```shell
   src/tools/fmt fmt
   ```

   On my laptop this takes about 2.5 seconds.

1. Of course, when you're writing code, 2.5 seconds is an eternity.
   Here's a non-exhaustive list of editor / IDE plugins that provide an
   integrated formatting experience.
   The in-editor formatting typically takes around 50ms.
   Depending on your plugin / editor,
   the formatting either happens automatically as you type, on save,
   or when you invoke it explicitly.

   [int.emacs]: https://clang.llvm.org/docs/ClangFormat.html#emacs-integration
   * LLVM's official [Emacs integration][int.emacs] provides the _`clang-format-region`_ macro.

   [int.vim]: https://clang.llvm.org/docs/ClangFormat.html#vim-integration
   * [Vim integration][int.vim] as documented by the official LLVM project

   [int.altvim]: https://github.com/rhysd/vim-clang-format
   * A community [VIM plugin][int.altvim] that looks promising and claims to be better than the official VIM integration

   [int.clion]: https://www.jetbrains.com/help/clion/clangformat-as-alternative-formatter.html
   * [Clion][int.clion] detects `.clang-format` automatically and switches to using it for the built-in "Reformat Code" and "Reformat File...".

   [int.vscode]: https://code.visualstudio.com/docs/cpp/cpp-ide#_code-formatting
   * Similar to CLion, [Visual Studio Code has it built in][int.vscode].

   [int.xcode8]: https://github.com/mapbox/XcodeClangFormat
   * [Plugin for Xcode 8+][int.xcode8]

### Annotating history
1. It's usually desirable to skip past formatting commits when doing a `git blame`. Fortunately, git blame has two options that are extremely helpful:
    1. `-w` ignore whitespace
    1. `--ignore-rev` / `--ignore-revs-file` accepts commits to skip while annotating

  When combined, `-w --ignore-revs-file` produces a perfectly clean annotation, unencumbered by a noisy history. This also gives us a peace of mind in changing our formats more frequently without worrying about hindering the `git blame` experience, rather than "do it once, set it in stone, and forget about it".

### Reformatting in-flight patches
ORCA is actively developed, that means when the "big bang" formatting change is merged,
there will inevitably be a number of patches that were started well before the formatting change,
and these patches will now need to conform to the new format.
The following steps are used to convert in-flight branches to the new format.

1. Fetch the inflight PR. Let's say its greenplum-db/gpdb#20042

   ```sh
   git fetch origin refs/pull/20042/head
   git checkout -b 20042-backup FETCH_HEAD
   ```

1. Duplicate the PR to a working branch. We'll reformat this branch:
   ```sh
   git checkout -b 20042-fmt
   ```

1. Rebase the working branch to just before the big bang.
   Normally, you'd do it like (but don't do this yet):

   ```sh
   git rebase 'origin/main^{/Format ORCA and GPOPT}~'
   ```

   Here `'origin/main^{/Format ORCA and GPOPT}'` is a Git notation that denotes
   the most recent commit from branch `origin/main' that contains the phrase
   "Format ORCA and GPOPT". The trailing tilde character is Git shorthand for "first parent".

1. But! We want to insert a precursor commit that will eventually disappear:

   ```sh
   git rebase --interactive 'origin/main^{/Format ORCA and GPOPT}~2'
   ```

   When we edit the "rebase todo" in an editor, make sure to insert the
   following action between the first two "pick" action items:

   ```diff
   --- /tmp/todo	2020-11-12 16:48:07.251333950 -0800
   +++ /tmp/todo2	2020-11-12 16:48:07.291333698 -0800
   @@ -1,7 +1,8 @@
    pick 87fd0149caf8f052 Penultimate commit on main
   +exec git checkout origin/main -- src/tools/fmt src/include/gpopt/.clang-format src/backend/gpopt/.clang-format src/backend/gporca/.clang-format; git commit -m 'this commit will disappear'
    pick 93235faf306f4ecb 1st commit from PR
    pick 2b7ad8b9fd1f222c 2nd commit from PR
    pick 5bc7ac02c1c78b8b Last commit from PR

    # Rebase f96bb1e290ef188e..5bc7ac02c1c78b8b onto c6ab7beee96e8bc7 (2 commands)
    #
   ```

   What happens here is that we inserted a placeholder commit that gets us the
   format script and format configuration while rebasing.

   This commit will disappear in the final history (read on)

1. Use `git filter-branch` to rewrite the branch history

   ```sh
   git filter-branch --force --tree-filter 'CLANG_FORMAT=clang-format-10 src/tools/fmt fmt; CLANG_FORMAT=clang-format-10 src/tools/fmt fmt' origin/main..
   ```

   Now we have reformatted every commit in this branch that's not in main (modulo the big bang commit).
   As a bonus, our placeholder commit should now be identical to the tip of main (the big bang formatting commit).

1. Now that we have reformatted the history, rebase on top of main

   ```sh
   git rebase origin/main
   ```

   Git should be smart enough to drop our placeholder commit, and now the history is all properly formatted.
