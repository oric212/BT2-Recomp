import unittest
import story_sweep


class StorySweepTests(unittest.TestCase):
    def test_cases_take_the_story_marker_and_list_parts_without_one(self):
        parts=[{'episode':0,'part':0,'story_marker':14},{'episode':0,'part':1,'story_marker':None},{'episode':2,'part':0,'story_marker':3},
               {'episode':2,'part':1,'story_marker':None,'direct_story':True}]
        self.assertEqual(story_sweep.cases(parts),(['0,0,marker:14','2,0,marker:3','2,1'],['0,1']))
        self.assertEqual(story_sweep.cases(parts,{2}),(['2,0,marker:3','2,1'],[]))

    def test_name_and_completion(self):
        self.assertEqual(story_sweep.name('3,12,marker:7'),'e03-p12')
        self.assertEqual(story_sweep.name('3,2'),'e03-p02')
        self.assertEqual(story_sweep.outcome({'starter_parts_completed':1,'starter_battles_cleared':1}),'cleared')
        self.assertEqual(story_sweep.outcome({'starter_parts_completed':1,'starter_battles_lost':1}),'lost')
        self.assertEqual(story_sweep.outcome({'starter_stuck':0x255580}),'stuck: 00255580')
        self.assertEqual(story_sweep.outcome({'frame_budget_reached':1}),'')


if __name__=='__main__':unittest.main()
