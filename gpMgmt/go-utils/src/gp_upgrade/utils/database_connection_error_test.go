package utils_test

import (
	"gp_upgrade/utils"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"github.com/pkg/errors"
)

var _ = Describe("error_mapper", func() {
	Describe("#GetExitCodeForError", func() {
		It("returns the correct error code", func() {
			rc := utils.GetExitCodeForError(errors.New("any error"))
			Expect(rc).To(Equal(1))

			var dbConnError utils.DatabaseConnectionError
			err := utils.GetExitCodeForError(dbConnError)
			Expect(err).To(Equal(65))
		})
		It("prints the correct error message", func() {
			var dbConnError utils.DatabaseConnectionError
			dbConnError.Parent = errors.New("foobar")
			Expect(dbConnError.Error()).To(Equal("Database Connection Error: foobar"))
		})
	})

})
