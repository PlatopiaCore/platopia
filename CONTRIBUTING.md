Contributing to Platopia Core
===========================

The Platopia Core project welcomes contributors!

This guide is intended to help developers contribute effectively to Platopia Core.


Platopia Core Development Philosophy
----------------------------------

Platopia Core aims for fast iteration and continuous integration.

This means that there should be quick turnaround for patches to be proposed,
reviewed, and committed. Changes should not sit in a queue for long.

Here are some tips to help keep the development working as intended:

- Keep each change small and self-contained.
- Reach out for a 1-on-1 review so things move quickly.
- Land the Diff quickly after it is accepted.
- Review Diffs from other developers as quickly as possible.
- Large changes should be broken into logical chunks that are easy to review,
and keep the code in a functional state.
- Do not mix moving stuff around with changing stuff. Do changes with renames
on their own.
- Sometimes you want to replace one subsystem by another implementation,
in which case it is not possible to do things incrementally. In such cases,
you keep both implementations in the codebase for a while, as described
[here](https://www.gamasutra.com/view/news/128325/Opinion_Parallel_Implementations.php)
- There are no "development" branches, all Diffs apply to the master 
branch, and should always improve it (no regressions).
- Don't break the build, it is important to keep master green as much as possible.
If a Diff is landed, and breaks the build, fix it quickly. If it cannot be fixed 
quickly, it should be reverted, and re-applied later when it no longer breaks the build.
- Automate as much as possible, and spend time on things only humans can do.

Here are some handy links for development practices aligned with Platopia Core:

- [Statement of Platopia Core Values and Visions](https://www.yours.org/content/platopia---our-values-and-vision-a282afaade7c)
- [Large Diffs Are Hurting Your Ability To Ship](https://medium.com/@kurtisnusbaum/large-diffs-are-hurting-your-ability-to-ship-e0b2b41e8acf)
- [Stacked Diffs: Keeping Phabricator Diffs Small](https://medium.com/@kurtisnusbaum/stacked-diffs-keeping-phabricator-diffs-small-d9964f4dcfa6)
- [Parallel Implementations](https://www.gamasutra.com/view/news/128325/Opinion_Parallel_Implementations.php)
- [The Pragmatic Programmer: From Journeyman to Master](https://www.amazon.com/Pragmatic-Programmer-Journeyman-Master/dp/020161622X)
- [Advantages of monolithic version control](https://danluu.com/monorepo/)


Getting set up with the Platopia Core Repository
----------------------------------------------

1. Create an account at `https://reviews.platopia.org/`

2. Install Git and Arcanist on your machine

Git documentation can be found at: `https://git-scm.com/`

Arcanist documentation can be found at:
`https://secure.phabricator.com/book/phabricator/article/arcanist_quick_start/`

And: `https://secure.phabricator.com/book/phabricator/article/arcanist/`

To install these packages on Debian or Ubuntu, type: `sudo apt-get install git arcanist`

3. If you do not already have an SSH key set up, follow these steps:

Type: `ssh-keygen -t rsa -b 4096 -C "your_email@example.com"`

Enter a file in which to save the key (/home/*username*/.ssh/id_rsa): [Press enter]

4. Upload your SSH public key to reviews.platopia.org

  - go to: `https://reviews.platopia.org/settings/user/*username*/page/ssh/`

  - Under "SSH Key Actions", Select "Upload Public Key"

Paste contents from: `/home/*username*/.ssh/id_rsa.pub`

5. Clone the repository and install Arcanist certificate:

```
git clone ssh://vcs@reviews.platopia.org:2221/source/platopia.git

cd platopia

arc install-certificate
```

Follow instructions provided by `arc install-certificate` to provide your API token.

6. Code formatting tools

If code formatting tools do not install automatically on your system, you
may have to install clang-format-3.8 and autopep8.


Working with The Platopia Core Repository
---------------------------------------

A typical workflow would be:

- Create a topic branch in Git for your changes

    git checkout -b 'my-topic-branch'

- Make your changes, and commit them

    git commit -a -m 'my-commit'

- Create a differential with Arcanist

    arc diff

You should add suggested reviewers and a test plan to the commit message.
Note that Arcanist is set up to look only at the most-recent commit message,
So all you changes for this Diff should be in one Git commit.

- For large changes, break them into several Diffs, as described in this
[guide](https://medium.com/@kurtisnusbaum/stacked-diffs-keeping-phabricator-diffs-small-d9964f4dcfa6).
You can also include "Depends on Dxxx" in the Arcanist message to indicate 
dependence on other Diffs.

- Log into Phabricator to see review and feedback.

- Make changes as suggested by the reviewers. You can simply edit the files
with my-topic-branch checked out, and then type `arc diff`. Arcanist will
give you the option to add uncommited changes. Or, alternatively, you can
commit the changes using `git commit -a --am` to add them to the last commit,
or squash multiple commits by typing `git rebase -i master`. If you squash, 
make sure the commit message has the information needed for arcanist (such 
as the Diff number, reviewers, etc.).

- Update your Diff by typing `arc diff` again.

- When reviewers approve your Diff, it should be listed as "ready to Land"
in Phabricator. When you want to commit your diff to the repository, check out
type my-topic-branch in git, then type `arc land`. You have now succesfully 
committed a change to the Platopia Core repository.


What to work on
---------------

If you are looking for a useful task to contribute to the project, a good place
to start is the list of tasks at `https://reviews.platopia.org/maniphest/`.
You can also get in touch with some of the developers in the open BTCForks Slack
channel at `https://btcforks.slack.com/`


Copyright
---------

By contributing to this repository, you agree to license your work under the
MIT license unless specified otherwise in `contrib/debian/copyright` or at
the top of the file itself. Any work contributed where you are not the original
author must contain its license header with the original author(s) and source.

