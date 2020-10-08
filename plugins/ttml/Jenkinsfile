node('master') {
    sh "git archive --remote=git@bitbucket.org:fluendo/flu-codec-ci.git master codecs-ci.groovy | tar -x -O > codecs-ci.groovy"
    def codecs = load 'codecs-ci.groovy'
    codecs.run_all()
}
