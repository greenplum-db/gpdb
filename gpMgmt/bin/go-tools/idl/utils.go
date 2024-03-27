package idl

// GetPrimarySegments is a helper function which returns a slice of pointers
// to Segment objects representing the primary segments in the MakeClusterRequest.
func (m *MakeClusterRequest) GetPrimarySegments() []*Segment {
	var primarySegs []*Segment
	for _, pair := range m.GpArray.SegmentArray {
		if pair.Primary != nil {
			primarySegs = append(primarySegs, pair.Primary)
		}
	}

	return primarySegs
}

// GetMirrorSegments is a helper function which returns a slice of pointers
// to Segment objects representing the mirror segments in the MakeClusterRequest.
func (m *MakeClusterRequest) GetMirrorSegments() []*Segment {
	var mirrorSegs []*Segment
	for _, pair := range m.GpArray.SegmentArray {
		if pair.Mirror != nil {
			mirrorSegs = append(mirrorSegs, pair.Mirror)
		}
	}

	return mirrorSegs
}
