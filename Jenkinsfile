node('master') {
    build(job: 'flu-codec-ci/master', parameters: [
            string(name: 'PLUGIN', value: 'ttml'),
            string(name: 'TAG', value: env.BRANCH_NAME),
            string(name: 'FLUENDO_CERBERO_BRANCH', value: env.BRANCH_NAME),
            booleanParam(name: 'IS_RELEASE', value: false)])
}
