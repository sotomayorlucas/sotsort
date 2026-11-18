# arXiv endorsement request (draft — edit before sending)

Subject: arXiv cs.DS endorsement request — stable in-place sorting, implementation and experiments

Dear Prof. [NAME],

I am a security researcher at BASE4 Security (Buenos Aires) and a computer science student at the
University of Buenos Aires. I would like to post a technical report to arXiv (cs.DS) and, as a
first-time submitter in that category, I need an endorsement.

The report describes SotSort, a stable sort built around a stable quicksort whose partition works
in blocks through a fixed buffer, with one code path that runs from O(1) extra memory up to a full
n-element buffer. It includes pseudocode, hand-written correctness proofs, and an experimental
comparison against blitsort, fluxsort, glidesort, driftsort, GrailSort and WikiSort. The claims
are deliberately narrow: measurements come from a single 2-vCPU cloud machine, the code is a
reconstruction of an earlier version, and the report lists where the algorithm loses.

Code, tests and raw measurements: [REPOSITORY URL]
Draft: [PDF LINK]

I chose to write to you because of [YOUR PAPER / WHY THIS PERSON]. If you are willing, the
endorsement code is [CODE FROM ARXIV]. I understand if you would rather not, and thank you for
your time either way.

Best regards,
Lucas Sotomayor
