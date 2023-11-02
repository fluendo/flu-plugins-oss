All
===

[ ] Check categories
[ ] Check descriptions
[ ] Use instance in `gst::debug!` macros
[ ] Fix all `unwraps()` that should be handled.
[ ] ASCII? diagram in the documentation
[ ] Fill the README
[ ] Right now, pipelines are not accepted as an option

Not prioritary:
[ ] Community error:  "Parent function `propose_allocation` failed"
[ ] Check caps negotiation and add unit test for it

SceneDetector
=============

[ ] Do not send the `gop_size` inside the custom event
[ ] Make `new_scene()` method more robust
[X] Rename `scenedetection` to `scenedetector`

Not prioritary:
[ ] Use buffer offset as buffer counter?

OutputSelector
==============

[ ] Move the pad probe from a lamda to a method?
[ ] In the pad probe, it'd be better to take the lock only once
[ ] In the pad probe, reduce the indentation level using `let...else` or `if let`
[ ] Leave the queue here or move them into hype?
[ ] Give the user access to internal queue's properties

Not prioritary:
[ ] Move the logic into Hype bin?

SceneColletor
=============

[X] Check how to do caps negotiation properly (still room from improvement here)
[ ] Check how to handle EOS properly
[ ] Better name for `aux` variable

Not prioritary:
[ ] Use LinkedList instead of Vec for sinkpads list?
[ ] Fix `request_pad()` to use the template argument

Hype
====

[ ] Test transcoding (add example to README)

Not prioritary:
[ ] Make the number of encoders dynamic
